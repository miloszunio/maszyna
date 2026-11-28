/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "world/trees/treeexport.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <functional>
#include <string>
#include <unordered_map>

namespace trees
{

namespace
{

// a point of the tree as one of a model: x the other way, the height third (as editorStructure.cpp writes them)
glm::vec3 model_axes(glm::vec3 const &Point)
{
	return {-Point.x, Point.z, Point.y};
}

// a number as short as it can be written to Digits places: the models are big, and most of their numbers are short
std::string compact(float const Value, int const Digits)
{
	auto text{std::format("{:.{}f}", Value, Digits)};
	if (auto const point{text.find('.')}; point != std::string::npos)
	{
		auto const last{text.find_last_not_of('0')};
		text.erase(last == point ? point : last + 1);
	}
	if (text == "-0")
		text = "0";
	return text;
}

glm::ivec3 mix(glm::ivec3 const &From, glm::ivec3 const &To, float const Share)
{
	glm::vec3 const from{From};
	glm::vec3 const to{To};
	return glm::ivec3{glm::vec3{std::lerp(from.r, to.r, Share), std::lerp(from.g, to.g, Share), std::lerp(from.b, to.b, Share)} + 0.5f};
}

struct submodel
{
	std::string name;
	glm::ivec3 colour{255};
	float near{0.f};
	float far{0.f};
};

// the triangles of a part the filter keeps, as a submodel; nothing is written when none is kept
void write_part(std::ostream &Out, submodel const &Model, mesh_part const &Part, std::function<bool(vertex const &)> const &Keep)
{
	std::vector<std::uint32_t> indices;
	std::vector<vertex> vertices;
	std::unordered_map<std::uint32_t, std::uint32_t> renumbered;
	auto const take{[&renumbered, &vertices, &Part](std::uint32_t const Index)
	                {
		                auto const [found, added]{renumbered.try_emplace(Index, static_cast<std::uint32_t>(vertices.size()))};
		                if (added)
			                vertices.push_back(Part.vertices[Index]);
		                return found->second;
	                }};
	for (std::size_t index = 0; index + 2 < Part.indices.size(); index += 3)
	{
		if (false == Keep(Part.vertices[Part.indices[index]]))
			continue;
		auto const a{take(Part.indices[index])};
		auto const b{take(Part.indices[index + 1])};
		auto const c{take(Part.indices[index + 2])};
		indices.insert(indices.end(), {a, b, c});
	}
	if (indices.empty())
		return;
	// the back of a leaf: the same triangles the other way round, facing the other way
	if (Part.two_sided)
	{
		auto const count{static_cast<std::uint32_t>(vertices.size())};
		for (std::uint32_t index = 0; index < count; ++index)
		{
			auto back{vertices[index]};
			back.normal = -back.normal;
			back.position -= vertices[index].normal * 0.0005f;
			vertices.push_back(back);
		}
		auto const triangles{indices.size()};
		for (std::size_t index = 0; index < triangles; index += 3)
			indices.insert(indices.end(), {indices[index] + count, indices[index + 2] + count, indices[index + 1] + count});
	}
	auto const &colour{Model.colour};
	Out << std::format("Parent: none\nType: Mesh\nName: {}\nAnim: false\nAmbient: {} {} {}\nDiffuse: {} {} {}\n", Model.name, colour.r, colour.g, colour.b, colour.r, colour.g, colour.b)
	    << "Specular: 0 0 0\nSelfIllum: false\nWire: false\nWireSize: 1\nOpacity: 1\nMap: none\n"
	    << std::format("MaxDistance: {}\nMinDistance: {}\n", Model.far, Model.near) << "Transform:\n 1 0 0 0\n 0 1 0 0\n 0 0 1 0\n 0 0 0 1\n\n"
	    << "NumIndices: " << indices.size() << "\n";
	for (std::size_t index = 0; index < indices.size(); index += 3)
		Out << std::format("{} {} {}\n", indices[index], indices[index + 1], indices[index + 2]);
	Out << "NumVerts: " << vertices.size() << "\n";
	for (auto const &point : vertices)
	{
		auto const position{model_axes(point.position)};
		auto const normal{model_axes(point.normal)};
		auto const tangent{model_axes(glm::vec3{point.tangent})};
		Out << compact(position.x, 3) << ' ' << compact(position.y, 3) << ' ' << compact(position.z, 3) << ' ' << compact(normal.x, 2) << ' ' << compact(normal.y, 2) << ' ' << compact(normal.z, 2)
		    << ' ' << compact(point.uv.x, 3) << ' ' << compact(point.uv.y, 3) << ' ' << compact(tangent.x, 2) << ' ' << compact(tangent.y, 2) << ' ' << compact(tangent.z, 2) << ' '
		    << compact(point.tangent.w, 0) << '\n';
	}
	Out << "\n";
}

} // namespace

season_look look_of(species const &Species, season const Season)
{
	season_look look;
	look.first = Species.leaf_summer;
	look.last = Species.leaf_summer;
	if (Species.evergreen)
	{
		look.last = mix(Species.leaf_summer, Species.leaf_spring, Season == season::spring ? 0.6f : 0.15f);
		return look;
	}
	switch (Season)
	{
	case season::spring:
		look.leaves = Species.flower_size > 0.f ? 0.45f : 0.8f;
		look.first = Species.leaf_spring;
		look.last = mix(Species.leaf_spring, Species.leaf_summer, 0.4f);
		look.flowers = Species.flower_size > 0.f;
		break;
	case season::autumn:
		look.leaves = 0.7f;
		look.first = mix(Species.leaf_summer, Species.leaf_autumn, 0.35f);
		look.last = Species.leaf_autumn;
		look.fruits = Species.fruit_size > 0.f;
		break;
	case season::winter:
		look.leaves = Species.keeps_dry_leaves ? 0.55f : 0.f;
		look.first = glm::ivec3{150, 112, 72};
		look.last = glm::ivec3{128, 95, 62};
		break;
	case season::summer:
	default:
		look.last = mix(Species.leaf_summer, Species.leaf_spring, 0.2f);
		look.fruits = Species.fruit_size > 0.f && Species.fruit_day > 0.f && Species.fruit_day < 220.f;
		break;
	}
	return look;
}

void write_t3d(std::ostream &Out, std::vector<tree_mesh> const &Levels, species const &Species, export_settings const &Settings)
{
	Out << std::format("// {} ({}), made by the tree generator of MaSzyna\n\n", Species.name, Species.id);
	auto const look{look_of(Species, Settings.time)};
	for (auto const &level : Levels)
	{
		auto const index{static_cast<std::size_t>(std::max(0, level.level))};
		auto const near{index == 0 || index > Settings.distances.size() ? 0.f : Settings.distances[index - 1]};
		auto const far{index < Settings.distances.size() ? Settings.distances[index] : 3000.f};
		auto const named{[&level, near, far](std::string_view const Name, glm::ivec3 const &Colour) { return submodel{std::format("lod{}_{}", level.level, Name), Colour, near, far}; }};
		if (auto const *bark{level.find(part::bark)})
			write_part(Out, named("bark", Species.bark), *bark, [](vertex const &) { return true; });
		if (auto const *leaves{level.find(part::leaves)}; leaves != nullptr && look.leaves > 0.f)
		{
			// three shades, by the rank of the leaves still on
			for (int shade = 0; shade < 3; ++shade)
			{
				auto const low{look.leaves * static_cast<float>(shade) / 3.f};
				auto const high{look.leaves * static_cast<float>(shade + 1) / 3.f};
				write_part(Out, named(std::format("leaves{}", shade), mix(look.first, look.last, (static_cast<float>(shade) + 0.5f) / 3.f)), *leaves,
				           [low, high](vertex const &Vertex) { return Vertex.rank >= low && Vertex.rank < high; });
			}
		}
		if (auto const *flowers{level.find(part::flowers)}; flowers != nullptr && look.flowers)
			write_part(Out, named("flowers", Species.flower), *flowers, [](vertex const &) { return true; });
		if (auto const *fruits{level.find(part::fruits)}; fruits != nullptr && look.fruits)
			write_part(Out, named("fruits", Species.fruit), *fruits, [](vertex const &) { return true; });
	}
}

} // namespace trees
