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
// true if the square from Low, Size wide, and the outline overlap: a corner of one inside the other, or two of their edges crossing
bool square_touches(glm::dvec2 const &Low, double const Size, std::vector<glm::dvec2> const &Outline)
{
	auto const high{Low + glm::dvec2{Size}};
	std::array<glm::dvec2, 4> const corners{Low, glm::dvec2{high.x, Low.y}, high, glm::dvec2{Low.x, high.y}};
	for (auto const &corner : corners)
		if (inside_outline(Outline, corner.x, corner.y))
			return true;
	for (auto const &p : Outline)
		if (p.x > Low.x && p.x < high.x && p.y > Low.y && p.y < high.y)
			return true;
	auto const cross = [](glm::dvec2 const &A, glm::dvec2 const &B) { return A.x * B.y - A.y * B.x; };
	auto const crossing = [&](glm::dvec2 const &A, glm::dvec2 const &B, glm::dvec2 const &C, glm::dvec2 const &D) {
		auto const r{B - A}, q{D - C};
		auto const denominator{cross(r, q)};
		if (std::abs(denominator) < 1e-12)
			return false;
		auto const t{cross(C - A, q) / denominator}, u{cross(C - A, r) / denominator};
		return t > 0.0 && t < 1.0 && u > 0.0 && u < 1.0;
	};
	for (std::size_t i = 0; i < Outline.size(); ++i)
		for (std::size_t j = 0; j < 4; ++j)
			if (crossing(Outline[i], Outline[(i + 1) % Outline.size()], corners[j], corners[(j + 1) % 4]))
				return true;
	return false;
}

std::pair<glm::dvec2, glm::dvec2> bounds_of(std::vector<glm::dvec2> const &Outline)
{
	glm::dvec2 low{Outline.front()}, high{Outline.front()};
	for (auto const &p : Outline)
	{
		low = glm::min(low, p);
		high = glm::max(high, p);
	}
	return {low, high};
}
} // namespace

std::vector<terrain_streamer::grid_cell> terrain_streamer::cells_under(std::vector<glm::dvec2> const &Outline)
{
	std::vector<grid_cell> cells;
	if (Outline.size() < 3)
		return cells;
	auto const [low, high]{bounds_of(Outline)};
	for (auto *chunk : chunks_in(low, high, true))
	{
		auto const &data{chunk->data()};
		if (false == data.valid())
			continue;
		auto const spacing{data.spacing()};
		auto const corner{data.corner()};
		auto const x0{std::clamp(static_cast<int>(std::floor((low.x - corner.x) / spacing)), 0, data.cells - 1)};
		auto const x1{std::clamp(static_cast<int>(std::floor((high.x - corner.x) / spacing)), 0, data.cells - 1)};
		auto const z0{std::clamp(static_cast<int>(std::floor((low.y - corner.y) / spacing)), 0, data.cells - 1)};
		auto const z1{std::clamp(static_cast<int>(std::floor((high.y - corner.y) / spacing)), 0, data.cells - 1)};
		for (int iz = z0; iz <= z1; ++iz)
			for (int ix = x0; ix <= x1; ++ix)
			{
				glm::dvec2 const at{corner.x + ix * spacing, corner.y + iz * spacing};
				if (false == square_touches(at, spacing, Outline))
					continue;
				cells.push_back({at, spacing, {data.height(ix, iz), data.height(ix + 1, iz), data.height(ix, iz + 1), data.height(ix + 1, iz + 1)}, chunk->key()});
			}
	}
	return cells;
}

bool terrain_streamer::cut_cells(std::vector<glm::dvec2> const &Outline, bool const Cut)
{
	if (Outline.size() < 3)
		return false;
	auto const [low, high]{bounds_of(Outline)};
	bool changed{false};
	for (auto *chunk : chunks_in(low - glm::dvec2{5.0}, high + glm::dvec2{5.0}, true))
	{
		editor_busy_progress(-1.0);
		auto const &current{chunk->data()};
		if (false == current.valid() || (false == Cut && false == current.has_holes()))
			continue;
		auto data{std::make_shared<heightmap::chunk_data>(current)};
		data->ensure_holes();
		auto const cells{data->cells};
		if (data->holes < 2 * cells)
		{
			// two samples a cell at least: one inside each cell, the cut of it not decided by its corners alone
			auto const finer{2 * cells};
			std::vector<std::uint8_t> mask(static_cast<std::size_t>(finer + 1) * (finer + 1));
			auto const step{heightmap::chunk_size / finer};
			auto const corner{data->corner()};
			for (int iz = 0; iz <= finer; ++iz)
				for (int ix = 0; ix <= finer; ++ix)
					mask[static_cast<std::size_t>(iz) * (finer + 1) + ix] = static_cast<std::uint8_t>(std::lround(255.0 * data->hole_value(corner.x + ix * step, corner.y + iz * step)));
			data->holes = finer;
			data->hole_mask = std::move(mask);
		}
		auto const spacing{data->spacing()};
		auto const corner{data->corner()};
		auto const ratio{data->holes / cells};
		auto const side{static_cast<std::size_t>(data->holes + 1)};
		// the cells of this chunk reached into, and one past each edge: a sample on the edge of the chunk sees the cell beyond
		auto const x0{static_cast<int>(std::floor((low.x - corner.x) / spacing)) - 1}, x1{static_cast<int>(std::floor((high.x - corner.x) / spacing)) + 1};
		auto const z0{static_cast<int>(std::floor((low.y - corner.y) / spacing)) - 1}, z1{static_cast<int>(std::floor((high.y - corner.y) / spacing)) + 1};
		if (x1 < -1 || z1 < -1 || x0 > cells || z0 > cells)
			continue;
		auto const width{x1 - x0 + 1};
		std::vector<char> reached(static_cast<std::size_t>(width) * (z1 - z0 + 1), 0);
		bool any{false};
		for (int iz = z0; iz <= z1; ++iz)
			for (int ix = x0; ix <= x1; ++ix)
				if (square_touches({corner.x + ix * spacing, corner.y + iz * spacing}, spacing, Outline))
				{
					reached[static_cast<std::size_t>(iz - z0) * width + (ix - x0)] = 1;
					any = true;
				}
		if (false == any)
			continue;
		auto const cut_cell = [&](int const Ix, int const Iz) {
			return Ix >= x0 && Ix <= x1 && Iz >= z0 && Iz <= z1 && reached[static_cast<std::size_t>(Iz - z0) * width + (Ix - x0)] != 0;
		};
		bool touched{false};
		auto const sx0{std::max(0, x0 * ratio)}, sx1{std::min(data->holes, (x1 + 1) * ratio)};
		auto const sz0{std::max(0, z0 * ratio)}, sz1{std::min(data->holes, (z1 + 1) * ratio)};
		for (int iz = sz0; iz <= sz1; ++iz)
			for (int ix = sx0; ix <= sx1; ++ix)
			{
				// the cells the sample belongs to: one inside a cell, two on a line of the grid, four at a grid point
				int const cx{ix / ratio}, cz{iz / ratio};
				std::array<int, 2> const xs{ix % ratio == 0 ? cx - 1 : cx, cx};
				std::array<int, 2> const zs{iz % ratio == 0 ? cz - 1 : cz, cz};
				int reached_count{0}, total{0};
				for (auto const zc : zs)
					for (auto const xc : xs)
					{
						++total;
						if (cut_cell(xc, zc))
							++reached_count;
					}
				if (reached_count == 0)
					continue;
				auto &sample{data->hole_mask[static_cast<std::size_t>(iz) * side + ix]};
				std::uint8_t value{0};
				if (Cut)
					value = static_cast<std::uint8_t>(std::max<int>(sample, reached_count == total ? 255 : 127));
				if (sample == value)
					continue;
				sample = value;
				touched = true;
			}
		if (false == touched)
			continue;
		data->compact_holes();
		remember(*chunk);
		chunk->replace(std::move(data), true, true);
		changed = true;
	}
	return changed;
}

bool terrain_streamer::height_at(double X, double Z, double &Height) const
{
	auto const *chunk{terrain_at(X, Z)};
	if (chunk == nullptr)
		return false;
	Height = chunk->height_at(X, Z);
	return true;
}

bool terrain_streamer::cut_at(double X, double Z) const
{
	auto const *chunk{terrain_at(X, Z)};
	return chunk != nullptr && chunk->data().hole_at(X, Z);
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
