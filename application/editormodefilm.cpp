/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// what a cab-view film saw beside the track laid along the route in the scenery, and the magic wand on the orthophoto

#include "stdafx.h"
#include "application/editormode.h"
#include "application/editoruilayer.h"
#include "application/editormodeshared.h"
#include "editor/editorFormat.hpp"
#include "model/AnimModel.h"
#include "simulation/simulation.h"
#include "utilities/Globals.h"
#include "utilities/Logs.h"
#include "utilities/translation.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <array>
#include <deque>
#include <functional>
#include <tuple>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <unordered_map>

namespace
{

// the classes kabina_meta.py writes
enum film_class : std::uint8_t
{
	none,
	ballast,
	grass,
	dry_grass,
	bushes,
	building,
	pole,
	earth
};

film_class film_class_of(std::string const &Name)
{
	static std::map<std::string, film_class> const names{{"tluczen", ballast}, {"trawa", grass}, {"trawa_sucha", dry_grass}, {"krzaki", bushes},
	                                                     {"budynek", building}, {"slup", pole}, {"ziemia", earth}};
	auto const found{names.find(Name)};
	return found != names.end() ? found->second : none;
}

// the axis of the tracks the train went along, in pieces where the scenery has them, and the points where the kilometres of the
// film are matched to the metres along it
struct film_route
{
	struct piece
	{
		std::vector<glm::dvec2> points;
		std::vector<double> along; // metres from the start of the piece, a value for each point
		std::vector<std::pair<double, double>> anchors; // metres along, kilometre of the film
	};
	std::vector<piece> pieces;

	bool read(std::string const &Path)
	{
		std::ifstream file{Path};
		if (false == file.good())
			return false;
		std::string line;
		while (std::getline(file, line))
		{
			std::istringstream words{line};
			std::string word;
			if (false == static_cast<bool>(words >> word) || word[0] == '#')
				continue;
			if (word == "kawalek")
				pieces.emplace_back();
			else if (false == pieces.empty() && word == "punkt")
			{
				double s, km;
				if (words >> s >> km)
					pieces.back().anchors.emplace_back(s, km);
			}
			else if (false == pieces.empty() && word == "xz")
			{
				double x, z;
				if (words >> x >> z)
					pieces.back().points.emplace_back(x, z);
			}
		}
		for (auto &piece : pieces)
		{
			piece.along.assign(piece.points.size(), 0.0);
			for (std::size_t i = 1; i < piece.points.size(); ++i)
				piece.along[i] = piece.along[i - 1] + glm::distance(piece.points[i - 1], piece.points[i]);
		}
		pieces.erase(std::remove_if(pieces.begin(), pieces.end(), [](piece const &Piece) { return Piece.points.size() < 2 || Piece.anchors.empty(); }), pieces.end());
		return false == pieces.empty();
	}

	// the kilometre of the film to the metres along the piece: between the matched points as they say, past them one to one
	static double to_along(piece const &Piece, double const Km)
	{
		auto const metres{Km * 1000.0};
		auto const &a{Piece.anchors};
		if (a.size() == 1 || metres <= a.front().second * 1000.0)
			return a.front().first + (metres - a.front().second * 1000.0);
		if (metres >= a.back().second * 1000.0)
			return a.back().first + (metres - a.back().second * 1000.0);
		for (std::size_t i = 1; i < a.size(); ++i)
			if (metres <= a[i].second * 1000.0)
			{
				auto const t{(metres - a[i - 1].second * 1000.0) / std::max(1e-6, (a[i].second - a[i - 1].second) * 1000.0)};
				return a[i - 1].first + (a[i].first - a[i - 1].first) * t;
			}
		return a.back().first;
	}

	// the point Lateral metres to the side (P right, L left as the train goes) of the axis at the kilometre of the film
	bool place(double const Km, double const Lateral, bool const Right, glm::dvec2 &Out) const
	{
		for (auto const &piece : pieces)
		{
			auto const s{to_along(piece, Km)};
			if (s < 0.0 || s > piece.along.back())
				continue;
			auto const index = [&](double const S) {
				auto const it{std::lower_bound(piece.along.begin(), piece.along.end(), S)};
				return static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(it - piece.along.begin(), 1, static_cast<std::ptrdiff_t>(piece.along.size()) - 1));
			};
			auto const i{index(s)};
			auto const t{(s - piece.along[i - 1]) / std::max(1e-9, piece.along[i] - piece.along[i - 1])};
			auto const point{piece.points[i - 1] + (piece.points[i] - piece.points[i - 1]) * t};
			// the way from a few metres on either side, so it doesn't jump on short sides
			auto const before{piece.points[index(s - 8.0) - 1]}, after{piece.points[index(s + 8.0)]};
			auto way{after - before};
			auto const length{glm::length(way)};
			if (length < 1e-6)
				return false;
			way /= length;
			// x of the scenery is the west, z the north: to the right of (dx, dz) is (-dz, dx)
			glm::dvec2 const right{-way.y, way.x};
			Out = point + right * (Right ? Lateral : -Lateral);
			return true;
		}
		return false;
	}
};

struct film_meta
{
	// the strips beside the track: side and the distance from the axis they're between; their classes every 5 m of the film
	struct strip
	{
		bool right{false};
		double from{0.0};
		double to{0.0};
		std::vector<film_class> classes;
		std::vector<float> cover;
	};
	double first{0.0}; // kilometre of the first entry
	double step{0.005};
	std::size_t count{0};
	std::vector<strip> strips;

	bool read(std::string const &Path)
	{
		std::ifstream file{Path};
		if (false == file.good())
			return false;
		struct row
		{
			double km;
			bool right;
			double from, to;
			film_class kind;
			float cover;
		};
		std::vector<row> rows;
		std::string line;
		while (std::getline(file, line))
		{
			if (line.empty() || line[0] == '#')
				continue;
			std::istringstream fields{line};
			row r;
			std::string side, kind;
			if (false == static_cast<bool>(fields >> r.km >> side >> r.from >> r.to >> kind >> r.cover))
				continue;
			r.right = side == "P";
			r.kind = film_class_of(kind);
			rows.push_back(r);
		}
		if (rows.empty())
			return false;
		first = std::numeric_limits<double>::max();
		double last{-first};
		for (auto const &r : rows)
		{
			first = std::min(first, r.km);
			last = std::max(last, r.km);
		}
		count = static_cast<std::size_t>(std::llround((last - first) / step)) + 1;
		std::map<std::tuple<bool, double, double>, std::size_t> found;
		for (auto const &r : rows)
		{
			auto const key{std::make_tuple(r.right, r.from, r.to)};
			auto it{found.find(key)};
			if (it == found.end())
			{
				it = found.emplace(key, strips.size()).first;
				strip s;
				s.right = r.right;
				s.from = r.from;
				s.to = r.to;
				s.classes.assign(count, none);
				s.cover.assign(count, 0.f);
				strips.push_back(std::move(s));
			}
			auto const i{static_cast<std::size_t>(std::llround((r.km - first) / step))};
			if (i < count)
			{
				// the city model takes the autumn grass for earth: beyond the track bed it's dry grass, on it the ballast
				auto kind{r.kind};
				if (kind == earth)
					kind = r.to <= 3.2 ? ballast : dry_grass;
				strips[it->second].classes[i] = kind;
				strips[it->second].cover[i] = r.cover;
			}
		}
		return true;
	}

	// the class most of the 25 m around has, then the pieces shorter than Shortest steps taken by the one before them
	void smooth(int const Shortest)
	{
		for (auto &s : strips)
		{
			auto smoothed{s.classes};
			for (std::size_t i = 0; i < count; ++i)
			{
				std::array<int, 8> votes{};
				for (std::size_t j = (i >= 2 ? i - 2 : 0); j <= std::min(count - 1, i + 2); ++j)
					if (s.classes[j] != none)
						++votes[s.classes[j]];
				auto const best{std::max_element(votes.begin() + 1, votes.end())};
				smoothed[i] = *best > 0 ? static_cast<film_class>(best - votes.begin()) : none;
			}
			std::size_t start{0};
			while (start < count)
			{
				auto end{start};
				while (end + 1 < count && smoothed[end + 1] == smoothed[start])
					++end;
				if (static_cast<int>(end - start + 1) < Shortest)
				{
					auto const neighbour{start > 0 ? smoothed[start - 1] : (end + 1 < count ? smoothed[end + 1] : smoothed[start])};
					std::fill(smoothed.begin() + start, smoothed.begin() + end + 1, neighbour);
				}
				start = end + 1;
			}
			s.classes = std::move(smoothed);
		}
	}
};

bool starts_with(std::string const &Text, char const *Prefix)
{
	return Text.rfind(Prefix, 0) == 0;
}

char const *const film_patch_prefix{"kabina "};
char const *const film_model_prefix{"kabina_"};

} // namespace

void editor_mode::remove_film()
{
	std::size_t models{0};
	std::vector<TAnimModel *> found;
	for (auto *model : simulation::Instances.sequence())
		if (model != nullptr && starts_with(model->name(), film_model_prefix))
			found.push_back(model);
	for (auto *model : found)
	{
		if (m_node == model)
		{
			m_node = nullptr;
			ui()->set_node(nullptr);
		}
		erase_model(model);
		++models;
	}
	auto all{m_streamer.patches()};
	std::vector<heightmap::surface_patch> cleared;
	std::vector<heightmap::surface_patch> kept;
	for (auto &patch : all)
		(starts_with(patch.name, film_patch_prefix) ? cleared : kept).push_back(std::move(patch));
	if (false == cleared.empty())
	{
		terrain_history_step();
		repaint_patches(std::move(kept), {}, cleared);
		terrain_history_step();
	}
	m_film_status = format(STR_C("Taken away: %zu surfaces and %zu trees and bushes laid out from the film"), cleared.size(), models);
}

void editor_mode::lay_out_film()
{
	auto &settings{m_film};
	film_route route;
	if (false == route.read(settings.route))
	{
		m_film_status = format(STR_C("Can't read the route %s"), settings.route);
		return;
	}
	film_meta meta;
	if (false == meta.read(settings.meta))
	{
		m_film_status = format(STR_C("Can't read the metadata %s"), settings.meta);
		return;
	}
	meta.smooth(std::max(1, static_cast<int>(std::lround(settings.shortest / 5.0f))));
	auto const km_of = [&](std::size_t const I) { return meta.first + static_cast<double>(I) * meta.step; };
	// what was laid out from a film before goes first
	remove_film();

	std::size_t surfaces{0}, plants{0};
	if (settings.surfaces)
	{
		std::array<std::string, 8> material{};
		material[dry_grass] = settings.dry;
		material[bushes] = settings.undergrowth;
		material[ballast] = settings.gravel;
		auto const &layers{m_streamer.manifest().layers};
		auto all{m_streamer.patches()};
		std::vector<std::size_t> changed;
		// for each side and class, the strips it's in, merged across: the cells with the same strips in a row make one surface,
		// cut every 100 m so a surface doesn't reach over half the scenery
		for (int side = 0; side < 2; ++side)
			for (auto const kind : {dry_grass, bushes, ballast})
			{
				if (material[kind].empty())
					continue;
				std::vector<film_meta::strip const *> strips;
				for (auto const &s : meta.strips)
					if (s.right == (side == 1) && s.to > settings.bed + 1e-3)
						strips.push_back(&s);
				std::sort(strips.begin(), strips.end(), [](auto const *A, auto const *B) { return A->from < B->from; });
				auto const mask = [&](std::size_t const I) {
					std::uint32_t bits{0};
					for (std::size_t k = 0; k < strips.size(); ++k)
						if (strips[k]->classes[I] == kind)
							bits |= 1u << k;
					return bits;
				};
				std::size_t start{0};
				while (start < meta.count)
				{
					auto const bits{mask(start)};
					auto end{start};
					while (end + 1 < meta.count && end - start + 1 < 20 && mask(end + 1) == bits)
						++end;
					// the strips next to each other merged: from the inner edge of the first to the outer one of the last
					for (std::size_t k = 0; bits != 0 && k < strips.size();)
					{
						if (0 == (bits & (1u << k)))
						{
							++k;
							continue;
						}
						auto last{k};
						while (last + 1 < strips.size() && (bits & (1u << (last + 1))) && std::abs(strips[last + 1]->from - strips[last]->to) < 0.01)
							++last;
						auto const inner{std::max(static_cast<double>(settings.bed), strips[k]->from) - 0.5};
						auto const outer{strips[last]->to + 0.5};
						std::vector<glm::dvec2> left, right;
						// overlapping the next one by half a cell, the transitions close the seams
						auto const from{km_of(start) - meta.step * 0.5}, to{km_of(end) + meta.step * 1.5};
						for (auto km{from};; km = std::min(to, km + meta.step))
						{
							glm::dvec2 a, b;
							if (route.place(km, inner, side == 1, a) && route.place(km, outer, side == 1, b))
							{
								left.push_back(a);
								right.push_back(b);
							}
							if (km >= to)
								break;
						}
						if (left.size() >= 2)
						{
							heightmap::surface_patch patch;
							patch.name = format("%s%s %c %.1f-%.1f km %07.3f", film_patch_prefix, kind == dry_grass ? "trawa_sucha" : kind == bushes ? "podszyt" : "zwir",
							                    side == 1 ? 'P' : 'L', inner + 0.5, outer - 0.5, km_of(start));
							patch.material = material[kind];
							for (auto const &layer : layers)
								if (layer.material == patch.material)
								{
									patch.size = layer.size;
									patch.rotation = layer.rotation;
									break;
								}
							patch.follow = true;
							patch.paint = true;
							patch.feather = settings.feather;
							patch.noise = settings.noise;
							patch.noise_scale = settings.noise_scale;
							patch.grass = -2; // the 3D grass as it is
							patch.outline = left;
							patch.outline.insert(patch.outline.end(), right.rbegin(), right.rend());
							all.push_back(std::move(patch));
							changed.push_back(all.size() - 1);
						}
						k = last + 1;
					}
					start = end + 1;
				}
			}
		surfaces = changed.size();
		if (false == changed.empty())
		{
			terrain_history_step();
			repaint_patches(std::move(all), changed);
			terrain_history_step();
		}
	}

	if (settings.plants)
	{
		// the templates of the groups of the node bank of the names given; the grasses among the bushes left out
		auto const templates = [&](char const *Group) {
			std::vector<std::string> result;
			auto &bank{ui()->nodebank()};
			auto const names{bank.group_names()};
			std::string wanted{Group};
			std::transform(wanted.begin(), wanted.end(), wanted.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
			for (std::size_t i = 0; i < names.size(); ++i)
			{
				auto name{names[i]};
				name.erase(0, name.find_first_not_of(" \t"));
				name.erase(name.find_last_not_of(" \t\r") + 1);
				std::transform(name.begin(), name.end(), name.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
				if (name != wanted)
					continue;
				for (auto const &entry : bank.group_templates(i))
				{
					auto lower{entry};
					std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char C) { return static_cast<char>(std::tolower(C)); });
					if (lower.find("model") != std::string::npos && lower.find("grass") == std::string::npos)
						result.push_back(entry);
				}
			}
			return result;
		};
		auto const trees{templates(settings.trees)};
		auto bushes_templates{templates(settings.bushes)};
		if (bushes_templates.empty())
			bushes_templates = trees;
		if (trees.empty())
			m_film_status = format(STR_C("No group %s in the node bank: no trees laid out"), settings.trees);
		else
		{
			std::mt19937 random{2026u};
			auto const uniform = [&](double const Low, double const High) { return std::uniform_real_distribution<double>{Low, High}(random); };
			std::size_t number{0};
			for (auto const &s : meta.strips)
			{
				if (s.to <= settings.nearest)
					continue;
				auto const inner{std::max(s.from, static_cast<double>(settings.nearest))};
				bool const forest{s.from >= 8.0};
				auto const density{forest ? settings.forest : settings.shrubs};
				for (std::size_t i = 0; i < meta.count; ++i)
				{
					if (s.classes[i] != bushes)
						continue;
					auto const expected{density * std::max(0.3f, s.cover[i]) * (5.0 * (s.to - inner)) / 10000.0};
					auto count{static_cast<int>(expected)};
					if (uniform(0.0, 1.0) < expected - count)
						++count;
					for (int n = 0; n < count; ++n)
					{
						auto const km{km_of(i) + uniform(0.0, meta.step)};
						auto const lateral{uniform(inner, s.to)};
						auto const yaw{uniform(0.0, 360.0)};
						auto const height{forest ? uniform(0.8, 1.25) : uniform(0.8, 1.2)};
						auto const width{height * uniform(0.82, 1.18)};
						auto const pick{uniform(0.0, 1.0)};
						glm::dvec2 point;
						if (false == route.place(km, lateral, s.right, point))
							continue;
						// the height from the terrain itself, its chunk loaded if it isn't: along a whole line most of them aren't
						double ground{0.0};
						if (false == m_streamer.height_at(point.x, point.y, ground, true))
							continue;
						glm::dvec3 const location{point.x, ground, point.y};
						if (false == simulation::Region->point_inside(location))
							continue;
						auto const &pool{forest ? trees : bushes_templates};
						auto const &src{pool[std::min(pool.size() - 1, static_cast<std::size_t>(pick * pool.size()))]};
						auto *model{simulation::State.create_model(src, film_model_prefix + std::to_string(number++), location)};
						if (model == nullptr)
							continue;
						// turned any way about the vertical, the height and the width apart: no two of them look the same
						m_editor.rotate(model, glm::vec3{0.0f, static_cast<float>(yaw), 0.0f}, 1);
						model->Scale(glm::vec3{static_cast<float>(width), static_cast<float>(height), static_cast<float>(width)});
						++plants;
					}
				}
			}
		}
	}
	m_film_status = format(STR_C("Laid out from the film: %zu surfaces painted into the terrain (Ctrl+Z takes them back) and %zu trees and bushes"), surfaces, plants);
	WriteLog("Editor film: " + m_film_status);
}

void editor_mode::render_film_ui()
{
	if (false == ImGui::CollapsingHeader(STR_C("From a cab-view film")))
		return;
	auto &settings{m_film};
	ImGui::TextWrapped("%s", STR_C("What kabina_meta.py saw beside the track in a film from the cab (.meta), laid along the route of the film over the tracks "
	                               "of the scenery (.trasa): the dry grass, the undergrowth and the gravel of the verge as surfaces painted into the terrain, "
	                               "the trees and the bushes as models of the node bank"));
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##filmmeta", STR_C("metadata (.meta)"), settings.meta, sizeof(settings.meta));
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##filmroute", STR_C("route (.trasa)"), settings.route, sizeof(settings.route));
	ImGui::Checkbox(STR_C("Surfaces##film"), &settings.surfaces);
	ImGui::SameLine();
	ImGui::Checkbox(STR_C("Trees and bushes##film"), &settings.plants);
	auto const material = [&](char const *Label, char *Buffer, std::size_t Size) {
		ImGui::PushID(Label);
		ImGui::SetNextItemWidth(180.0f);
		ImGui::InputText(Label, Buffer, Size);
		std::string dropped;
		if (material_drop(dropped))
			std::snprintf(Buffer, Size, "%s", dropped.c_str());
		ImGui::SameLine();
		auto const &layers{m_streamer.manifest().layers};
		ImGui::BeginDisabled(layers.empty());
		if (ImGui::SmallButton(STR_C("from the palette")))
			std::snprintf(Buffer, Size, "%s", layers[std::clamp(m_terrain_layer, 0, static_cast<int>(layers.size()) - 1)].material.c_str());
		ImGui::EndDisabled();
		ImGui::PopID();
	};
	if (settings.surfaces)
	{
		ImGui::SeparatorText(STR_C("Materials##film"));
		material(STR_C("dry grass##film"), settings.dry, sizeof(settings.dry));
		material(STR_C("undergrowth of the bushes##film"), settings.undergrowth, sizeof(settings.undergrowth));
		material(STR_C("gravel of the verge##film"), settings.gravel, sizeof(settings.gravel));
		item_tooltip("Empty: that class isn't painted. The green grass stays the grass of the terrain, the track bed (up to the distance below) is left alone");
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Track bed up to (m)##film"), &settings.bed, 0.1f, 0.0f, 10.0f, "%.1f");
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Shortest piece (m)##film"), &settings.shortest, 1.0f, 5.0f, 200.0f, "%.0f");
		item_tooltip("Pieces shorter than this the one before takes: no tiles every 5 m");
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Transition (m)##film"), &settings.feather, 0.1f, 0.0f, 20.0f, "%.1f");
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Noise of the edge (m)##film"), &settings.noise, 0.05f, 0.0f, 10.0f, "%.2f");
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Waves of the noise (m)##film"), &settings.noise_scale, 0.1f, 0.5f, 50.0f, "%.1f");
	}
	if (settings.plants)
	{
		ImGui::SeparatorText(STR_C("Trees and bushes##film"));
		ImGui::SetNextItemWidth(180.0f);
		ImGui::InputText(STR_C("trees: group of the node bank##film"), settings.trees, sizeof(settings.trees));
		ImGui::SetNextItemWidth(180.0f);
		ImGui::InputText(STR_C("bushes: group of the node bank##film"), settings.bushes, sizeof(settings.bushes));
		item_tooltip("Every group of the name; the grasses in it left out. The trees from 8 m off the axis where the film saw bushes, the bushes nearer");
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Trees a hectare##film"), &settings.forest, 5.0f, 0.0f, 2000.0f, "%.0f");
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Bushes a hectare##film"), &settings.shrubs, 5.0f, 0.0f, 2000.0f, "%.0f");
		ImGui::SetNextItemWidth(120.0f);
		ImGui::DragFloat(STR_C("Nearest to the axis (m)##film"), &settings.nearest, 0.1f, 2.0f, 20.0f, "%.1f");
		item_tooltip("Nothing nearer: the clearance and the poles of the catenary");
	}
	ImGui::Spacing();
	if (main_button(STR_C("Lay out along the route##film"), ImVec2(-FLT_MIN, tall_button_height())))
		lay_out_film();
	item_tooltip("What was laid out from a film before is taken away first. Over a whole line it takes a while: the editor doesn't answer meanwhile.\n"
	             "The surfaces go back with Ctrl+Z, the trees with the button below");
	if (ImGui::Button(STR_C("Take away what was laid out from the film##film"), ImVec2(-FLT_MIN, 0.0f)))
		remove_film();
	if (false == m_film_status.empty())
		ImGui::TextWrapped("%s", m_film_status.c_str());
}

// ------------------------------------------------------------------------------------------------------------- the magic wand

bool editor_mode::wand_select(glm::dvec3 const &World)
{
	auto const &settings{m_wand};
	if (false == m_orthophoto.enabled())
	{
		m_wand_status = STR_C("The magic wand reads the orthophoto: turn it on");
		return false;
	}
	auto const step{std::clamp(static_cast<double>(settings.detail), 0.1, 5.0)};
	auto const reach{std::clamp(static_cast<double>(settings.radius), 10.0, 1500.0)};
	glm::dvec2 const centre{World.x, World.z};
	glm::dvec2 const low{centre - glm::dvec2{reach}}, high{centre + glm::dvec2{reach}};
	int width{0}, height{0};
	std::vector<std::uint8_t> rgba;
	if (false == m_orthophoto.sample(low, high, step, width, height, rgba))
	{
		m_wand_status = STR_C("No orthophoto in the cache here yet: wait until it shows, or download the area");
		return false;
	}
	auto const at = [&](int const X, int const Y) { return static_cast<std::size_t>(Y) * width + X; };
	// smoothed a little, so the grain of the photo doesn't stop the fill
	std::vector<glm::vec3> colour(static_cast<std::size_t>(width) * height);
	std::vector<char> known(colour.size(), 0);
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			glm::vec3 sum{0.0f};
			int n{0};
			for (int dy = -1; dy <= 1; ++dy)
				for (int dx = -1; dx <= 1; ++dx)
				{
					auto const xx{x + dx}, yy{y + dy};
					if (xx < 0 || yy < 0 || xx >= width || yy >= height || rgba[at(xx, yy) * 4 + 3] == 0)
						continue;
					auto const *p{&rgba[at(xx, yy) * 4]};
					sum += glm::vec3{p[0], p[1], p[2]};
					++n;
				}
			if (n > 0 && rgba[at(x, y) * 4 + 3] != 0)
			{
				colour[at(x, y)] = sum / static_cast<float>(n);
				known[at(x, y)] = 1;
			}
		}
	auto const seed_x{std::clamp(static_cast<int>((centre.x - low.x) / step), 0, width - 1)};
	auto const seed_y{std::clamp(static_cast<int>((centre.y - low.y) / step), 0, height - 1)};
	if (0 == known[at(seed_x, seed_y)])
	{
		m_wand_status = STR_C("No orthophoto in the cache under the click");
		return false;
	}
	// the colour clicked: of the few pixels around it
	glm::vec3 seed{0.0f};
	int seeds{0};
	for (int dy = -2; dy <= 2; ++dy)
		for (int dx = -2; dx <= 2; ++dx)
		{
			auto const xx{std::clamp(seed_x + dx, 0, width - 1)}, yy{std::clamp(seed_y + dy, 0, height - 1)};
			if (known[at(xx, yy)])
			{
				seed += colour[at(xx, yy)];
				++seeds;
			}
		}
	seed /= static_cast<float>(std::max(1, seeds));
	// the distance of colours as the eye takes it, roughly: green counts the most
	auto const tolerance{settings.tolerance * 2.55f};
	auto const close = [&](glm::vec3 const &C) {
		auto const d{C - seed};
		return std::sqrt(0.3f * d.r * d.r + 0.59f * d.g * d.g + 0.11f * d.b * d.b) <= tolerance;
	};
	std::vector<char> in(colour.size(), 0);
	std::deque<std::pair<int, int>> queue{{seed_x, seed_y}};
	in[at(seed_x, seed_y)] = 1;
	bool edge{false};
	std::size_t filled{0};
	while (false == queue.empty())
	{
		auto const [x, y]{queue.front()};
		queue.pop_front();
		++filled;
		if (x == 0 || y == 0 || x == width - 1 || y == height - 1)
			edge = true;
		for (auto const &d : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}})
		{
			auto const xx{x + d.first}, yy{y + d.second};
			if (xx < 0 || yy < 0 || xx >= width || yy >= height)
				continue;
			auto const i{at(xx, yy)};
			if (in[i] || 0 == known[i] || false == close(colour[i]))
				continue;
			in[i] = 1;
			queue.emplace_back(xx, yy);
		}
	}
	// the holes in it (a tree in a field, a car on a road) filled: whatever the outside doesn't reach
	if (settings.holes)
	{
		std::vector<char> outside(in.size(), 0);
		std::deque<std::pair<int, int>> open;
		auto const start = [&](int const X, int const Y) {
			if (0 == in[at(X, Y)] && 0 == outside[at(X, Y)])
			{
				outside[at(X, Y)] = 1;
				open.emplace_back(X, Y);
			}
		};
		for (int x = 0; x < width; ++x)
		{
			start(x, 0);
			start(x, height - 1);
		}
		for (int y = 0; y < height; ++y)
		{
			start(0, y);
			start(width - 1, y);
		}
		while (false == open.empty())
		{
			auto const [x, y]{open.front()};
			open.pop_front();
			for (auto const &d : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}})
			{
				auto const xx{x + d.first}, yy{y + d.second};
				if (xx >= 0 && yy >= 0 && xx < width && yy < height)
					start(xx, yy);
			}
		}
		for (std::size_t i = 0; i < in.size(); ++i)
			if (0 == outside[i])
				in[i] = 1;
	}
	// the outline: the sides of the pixels between in and out, joined, the longest loop of them
	std::unordered_multimap<std::int64_t, std::pair<std::int64_t, std::size_t>> edges;
	auto const corner = [&](int const X, int const Y) { return static_cast<std::int64_t>(Y) * (width + 1) + X; };
	std::vector<std::pair<std::int64_t, std::int64_t>> list;
	auto const inside = [&](int const X, int const Y) { return X >= 0 && Y >= 0 && X < width && Y < height && in[at(X, Y)]; };
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			if (false == inside(x, y))
				continue;
			if (false == inside(x, y - 1))
				list.emplace_back(corner(x, y), corner(x + 1, y));
			if (false == inside(x + 1, y))
				list.emplace_back(corner(x + 1, y), corner(x + 1, y + 1));
			if (false == inside(x, y + 1))
				list.emplace_back(corner(x + 1, y + 1), corner(x, y + 1));
			if (false == inside(x - 1, y))
				list.emplace_back(corner(x, y + 1), corner(x, y));
		}
	for (std::size_t i = 0; i < list.size(); ++i)
		edges.emplace(list[i].first, std::make_pair(list[i].second, i));
	std::vector<char> used(list.size(), 0);
	std::vector<std::int64_t> best;
	for (std::size_t i = 0; i < list.size(); ++i)
	{
		if (used[i])
			continue;
		std::vector<std::int64_t> loop{list[i].first};
		used[i] = 1;
		auto current{list[i].second};
		while (current != loop.front())
		{
			loop.push_back(current);
			bool moved{false};
			auto const range{edges.equal_range(current)};
			for (auto it{range.first}; it != range.second; ++it)
				if (0 == used[it->second.second])
				{
					used[it->second.second] = 1;
					current = it->second.first;
					moved = true;
					break;
				}
			if (false == moved)
				break;
		}
		if (loop.size() > best.size())
			best = std::move(loop);
	}
	if (best.size() < 4)
	{
		m_wand_status = STR_C("Nothing of that colour around the click: raise the tolerance");
		return false;
	}
	std::vector<glm::dvec2> points;
	points.reserve(best.size());
	for (auto const c : best)
		points.emplace_back(low.x + static_cast<double>(c % (width + 1)) * step, low.y + static_cast<double>(c / (width + 1)) * step);
	// the steps of the pixels smoothed out, then the points on near straight sides dropped
	for (int pass = 0; pass < 2; ++pass)
	{
		std::vector<glm::dvec2> smooth(points.size());
		for (std::size_t i = 0; i < points.size(); ++i)
			smooth[i] = (points[(i + points.size() - 1) % points.size()] + points[i] * 2.0 + points[(i + 1) % points.size()]) * 0.25;
		points = std::move(smooth);
	}
	auto const tolerance_line{std::max(step, static_cast<double>(settings.simplify))};
	std::vector<char> keep(points.size(), 0);
	std::function<void(std::size_t, std::size_t)> simplify = [&](std::size_t const A, std::size_t const B) {
		if (B <= A + 1)
			return;
		auto const a{points[A]}, b{points[B]};
		auto const ab{b - a};
		auto const length2{glm::dot(ab, ab)};
		double worst{-1.0};
		std::size_t index{A};
		for (auto i = A + 1; i < B; ++i)
		{
			auto const t{length2 > 0.0 ? std::clamp(glm::dot(points[i] - a, ab) / length2, 0.0, 1.0) : 0.0};
			auto const d{glm::distance(points[i], a + ab * t)};
			if (d > worst)
			{
				worst = d;
				index = i;
			}
		}
		if (worst > tolerance_line)
		{
			keep[index] = 1;
			simplify(A, index);
			simplify(index, B);
		}
	};
	// a closed loop: split at the point farthest from the first
	std::size_t farthest{0};
	for (std::size_t i = 1; i < points.size(); ++i)
		if (glm::distance(points[i], points[0]) > glm::distance(points[farthest], points[0]))
			farthest = i;
	keep[0] = keep[farthest] = 1;
	simplify(0, farthest);
	points.push_back(points[0]);
	keep.push_back(0);
	simplify(farthest, points.size() - 1);
	points.pop_back();
	keep.pop_back();
	std::vector<glm::dvec2> outline;
	for (std::size_t i = 0; i < points.size(); ++i)
		if (keep[i])
			outline.push_back(points[i]);
	if (outline.size() < 3)
	{
		m_wand_status = STR_C("The area picked is too small: lower the detail or raise the tolerance");
		return false;
	}
	m_paint_outline.clear();
	for (auto const &p : outline)
		m_paint_outline.push_back(placement_on_ground({p.x, World.y, p.y}));
	m_outline_shaped_key.clear();
	auto const area{static_cast<double>(filled) * step * step};
	m_wand_status = format(STR_C("Picked %.0f m2, %zu points of the outline%s"), area, outline.size(),
	                       edge ? STR_C(": it reached the edge of the square read, raise the reach") : "");
	return true;
}

void editor_mode::render_wand_options()
{
	auto &settings{m_wand};
	ImGui::Checkbox(STR_C("Magic wand on the orthophoto##wand"), &settings.on);
	item_tooltip("LMB on the orthophoto picks the area of about the colour clicked around it and makes it the outline, instead of a point: "
	             "a field, a meadow, a wood, a road. Best looked at from above (the top view)");
	if (false == settings.on)
		return;
	ImGui::SetNextItemWidth(120.0f);
	ImGui::SliderFloat(STR_C("Tolerance##wand"), &settings.tolerance, 1.0f, 60.0f, "%.0f");
	item_tooltip("How much the colour may differ from the one clicked: low - only the very same, high - also the shades around it");
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Reach (m)##wand"), &settings.radius, 5.0f, 20.0f, 1500.0f, "%.0f");
	item_tooltip("The half side of the square around the click the photo is read in; an area larger than it is cut at its edge");
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Detail (m)##wand"), &settings.detail, 0.05f, 0.1f, 5.0f, "%.2f");
	item_tooltip("A pixel of the photo read every that many metres: smaller - finer outline, slower over a large reach");
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat(STR_C("Straightening (m)##wand"), &settings.simplify, 0.05f, 0.1f, 10.0f, "%.2f");
	item_tooltip("Points of the outline closer than that to a straight side are dropped");
	ImGui::Checkbox(STR_C("Fill the holes##wand"), &settings.holes);
	item_tooltip("A tree in a field, a car on a road: whatever lies inside the area picked is taken with it");
	if (false == m_wand_status.empty())
		ImGui::TextWrapped("%s", m_wand_status.c_str());
}
