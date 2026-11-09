/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// our own parts of the terrain streamer (ficzery2): the height looked up with the chunk loaded, the georeference, the order and
// the hiding of the layers of the palette, the paint cleared over the whole terrain and one material replaced within an outline.
// Kept out of editorTerrainStreamer.cpp so the merges of the others' work touch it less

#include "stdafx.h"
#include <cstdio>
#include "editor/editorTerrainStreamer.hpp"
#include "model/vertex.h"
#include "rendering/renderer.h"
#include "scene/scene.h"
#include "simulation/simulation.h"
#include "utilities/Globals.h"
#include "utilities/Logs.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>

// how far the heavy work of the editor got (application/editormode.cpp); below 0 only keeps the window answering
bool editor_busy_progress(double Fraction);

namespace
{
// true if the point lies within the outline (x, z); the same test as editorTerrainStreamer.cpp has for itself
bool inside_outline(std::vector<glm::dvec2> const &Outline, double const X, double const Z)
{
	bool inside{false};
	for (std::size_t i = 0, j = Outline.size() - 1; i < Outline.size(); j = i++)
	{
		auto const &a{Outline[i]};
		auto const &b{Outline[j]};
		if ((a.y > Z) != (b.y > Z) && X < (b.x - a.x) * (Z - a.y) / (b.y - a.y) + a.x)
			inside = !inside;
	}
	return inside;
}
} // namespace

bool terrain_streamer::height_at(double X, double Z, double &Height) const
{
	auto const *chunk{terrain_at(X, Z)};
	if (chunk == nullptr)
		return false;
	Height = chunk->height_at(X, Z);
	return true;
}

void terrain_streamer::georeference(double const North, double const East)
{
	if (m_manifest.georeferenced && m_manifest.origin_north == North && m_manifest.origin_east == East)
		return;
	remember_manifest();
	m_manifest.georeferenced = North > 0.0 && East > 0.0;
	m_manifest.origin_north = North;
	m_manifest.origin_east = East;
	m_manifestchanged = true;
}

bool terrain_streamer::hide_layer(std::uint16_t const Layer, std::uint16_t const Into)
{
	auto &layers{m_manifest.layers};
	if (Layer == 0 || Layer >= layers.size() || Into >= layers.size() || Into == Layer || layers[Layer].hidden || layers[Into].hidden)
		return false;
	remember_manifest();
	auto &entry{layers[Layer]};
	entry.material = layers[Into].material;
	entry.size = layers[Into].size;
	entry.rotation = layers[Into].rotation;
	entry.hidden = true;
	m_manifestchanged = true;
	m_palette.erase(Layer);
	for (auto &chunk : m_chunks)
		if (chunk.second->data().slot_of(Layer) >= 0)
			chunk.second->m_paintedversion = 0;
	return true;
}

std::vector<std::uint16_t> terrain_streamer::layer_order() const
{
	std::vector<std::uint16_t> order;
	auto const &layers{m_manifest.layers};
	for (std::size_t i = 0; i < layers.size(); ++i)
		if (false == layers[i].hidden)
			order.push_back(static_cast<std::uint16_t>(i));
	auto const place = [&](std::uint16_t const Index) { return layers[Index].place >= 0 ? layers[Index].place : static_cast<int>(Index); };
	std::stable_sort(order.begin(), order.end(), [&](std::uint16_t const A, std::uint16_t const B) { return place(A) < place(B); });
	return order;
}

void terrain_streamer::move_layer(std::uint16_t const Layer, int const Step)
{
	auto order{layer_order()};
	auto const at{std::find(order.begin(), order.end(), Layer)};
	if (at == order.end() || Step == 0)
		return;
	auto const from{static_cast<int>(at - order.begin())};
	auto const to{std::clamp(from + Step, 0, static_cast<int>(order.size()) - 1)};
	if (to == from)
		return;
	remember_manifest();
	order.erase(order.begin() + from);
	order.insert(order.begin() + to, Layer);
	// the places written anew for all of the listed entries, so the order is kept as it's seen
	for (std::size_t i = 0; i < order.size(); ++i)
		m_manifest.layers[order[i]].place = static_cast<int>(i);
	m_manifestchanged = true;
}

void terrain_streamer::patches(std::vector<heightmap::surface_patch> Patches)
{
	remember_manifest();
	m_manifest.patches = std::move(Patches);
	m_manifestchanged = true;
	m_waterchanged = true;
}

std::vector<heightmap::pack_key> terrain_streamer::pack_keys() const
{
	std::vector<heightmap::pack_key> result;
	std::error_code ec;
	for (auto const &entry : std::filesystem::directory_iterator(m_directory, ec))
	{
		if (entry.path().extension() != ".tch")
			continue;
		int x{0}, z{0};
		if (std::sscanf(entry.path().stem().string().c_str(), "%d_%d", &x, &z) == 2)
			result.push_back({x, z});
	}
	return result;
}

std::size_t terrain_streamer::clear_paint(heightmap::pack_key const &Pack, std::uint16_t const Layer)
{
	std::vector<chunk_key> keys;
	{
		heightmap::pack_file file;
		std::lock_guard<std::mutex> lock{m_filemutex};
		if (file.open(pack_path(Pack)))
			keys = file.chunks();
	}
	for (auto const &entry : m_chunks)
		if (heightmap::pack_of(entry.first) == Pack && std::find(keys.begin(), keys.end(), entry.first) == keys.end())
			keys.push_back(entry.first);
	std::size_t cleared{0};
	for (auto const &key : keys)
	{
		auto *chunk{m_chunks.count(key) > 0 ? m_chunks.at(key).get() : load_now(key)};
		if (chunk == nullptr)
			continue;
		auto const &current{chunk->data()};
		if (current.paint <= 0 && current.layers[0] == Layer)
			continue;
		auto data{std::make_shared<heightmap::chunk_data>(current)};
		if (data->paint > 0 && data->layer_count() > 1)
			++cleared;
		data->paint = 0;
		data->weights.clear();
		data->layers.fill(heightmap::no_layer);
		data->layers[0] = Layer;
		chunk->replace(std::move(data), false, true);
	}
	return cleared;
}

std::pair<std::size_t, std::size_t> terrain_streamer::replace_in_area(std::vector<glm::dvec2> const &Outline, std::uint16_t const From, std::uint16_t const To)
{
	std::pair<std::size_t, std::size_t> result{0, 0};
	if (Outline.size() < 3 || From == To)
		return result;
	glm::dvec2 low{Outline.front()}, high{Outline.front()};
	for (auto const &point : Outline)
	{
		low = glm::min(low, point);
		high = glm::max(high, point);
	}
	for (auto *chunk : chunks_in(low, high, true))
	{
		editor_busy_progress(-1.0);
		auto const &current{chunk->data()};
		auto const from{current.slot_of(From)};
		if (from < 0)
			continue;
		auto const corner{current.corner()};
		auto const whole{inside_outline(Outline, corner.x, corner.y) && inside_outline(Outline, corner.x + heightmap::chunk_size, corner.y) &&
		                 inside_outline(Outline, corner.x, corner.y + heightmap::chunk_size) && inside_outline(Outline, corner.x + heightmap::chunk_size, corner.y + heightmap::chunk_size)};
		auto data{std::make_shared<heightmap::chunk_data>(current)};
		auto to{data->slot_of(To)};
		if (to < 0 && whole)
		{
			// all of the chunk: the entry itself swapped, no room needed
			data->layers[from] = To;
			remember(*chunk);
			chunk->replace(std::move(data), false, true);
			++result.first;
			continue;
		}
		data->ensure_paint();
		if (to < 0)
		{
			data->compact_layers();
			data->ensure_paint();
			if (data->layer_count() >= heightmap::max_chunk_layers)
			{
				++result.second;
				continue;
			}
			to = static_cast<int>(data->layer_count());
			data->layers[to] = To;
			data->weights.resize(data->weights.size() + static_cast<std::size_t>(data->paint + 1) * (data->paint + 1), 0);
		}
		auto const slot_from{data->slot_of(From)};
		auto const samples{data->paint};
		auto const plane{static_cast<std::size_t>(samples + 1) * (samples + 1)};
		auto const step{heightmap::chunk_size / samples};
		bool changed{false};
		for (int iz = 0; iz <= samples; ++iz)
			for (int ix = 0; ix <= samples; ++ix)
			{
				if (false == inside_outline(Outline, corner.x + ix * step, corner.y + iz * step))
					continue;
				auto const sample{static_cast<std::size_t>(iz) * (samples + 1) + ix};
				auto &a{data->weights[slot_from * plane + sample]};
				if (a == 0)
					continue;
				auto &b{data->weights[static_cast<std::size_t>(to) * plane + sample]};
				b = static_cast<std::uint8_t>(std::min(255, b + a));
				a = 0;
				changed = true;
			}
		if (false == changed)
			continue;
		data->compact_layers();
		remember(*chunk);
		chunk->replace(std::move(data), false, true);
		++result.first;
	}
	return result;
}
