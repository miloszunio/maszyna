/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "editor/editorStructure.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <tuple>

namespace structure
{

namespace
{

glm::dvec3 const kUp{0.0, 1.0, 0.0};

glm::dvec3 normal_of(glm::dvec3 const &A, glm::dvec3 const &B, glm::dvec3 const &C)
{
	auto const normal{glm::cross(B - A, C - A)};
	return glm::length(normal) > 1e-12 ? glm::normalize(normal) : kUp;
}

// the place of a point for the image: a face lying flat is mapped by its plan, a standing one by the way it runs and the height
glm::vec2 uv_of(glm::dvec3 const &Point, glm::dvec3 const &Normal, double const Tile)
{
	if (std::abs(Normal.y) > 0.7)
		return {static_cast<float>(Point.x / Tile), static_cast<float>(Point.z / Tile)};
	glm::dvec2 const along{glm::normalize(glm::dvec2{-Normal.z, Normal.x})};
	return {static_cast<float>((Point.x * along.x + Point.z * along.y) / Tile), static_cast<float>(Point.y / Tile)};
}

// a point of the scenery as one of the model: x the other way, the height third
glm::dvec3 model_axes(glm::dvec3 const &Point)
{
	return {-Point.x, Point.z, Point.y};
}

// the normals of the points of the part: of each triangle, or shared by the triangles meeting at a point
std::vector<glm::dvec3> normals_of(part const &Part)
{
	std::vector<glm::dvec3> normals(Part.points.size());
	for (std::size_t i = 0; i + 2 < Part.points.size(); i += 3)
	{
		auto const normal{normal_of(Part.points[i], Part.points[i + 1], Part.points[i + 2])};
		normals[i] = normal;
		normals[i + 1] = normal;
		normals[i + 2] = normal;
	}
	if (false == Part.smooth)
		return normals;
	using key = std::tuple<long long, long long, long long>;
	auto const key_of = [](glm::dvec3 const &Point) { return key{std::llround(Point.x * 1000.0), std::llround(Point.y * 1000.0), std::llround(Point.z * 1000.0)}; };
	std::map<key, glm::dvec3> sums;
	for (std::size_t i = 0; i < Part.points.size(); ++i)
		sums[key_of(Part.points[i])] += normals[i];
	for (std::size_t i = 0; i < Part.points.size(); ++i)
		if (auto const sum{sums[key_of(Part.points[i])]}; glm::length(sum) > 1e-9)
			normals[i] = glm::normalize(sum);
	return normals;
}

void write_part(std::ostream &Out, part const &Part, std::string const &Parent, glm::dvec3 const &Origin)
{
	Out << "Parent: " << Parent << "\nType: Mesh\nName: " << Part.name << "\nAnim: false\n"
	    << "Ambient: " << Part.colour.r << ' ' << Part.colour.g << ' ' << Part.colour.b << "\n"
	    << "Diffuse: " << Part.colour.r << ' ' << Part.colour.g << ' ' << Part.colour.b << "\n"
	    << "Specular: 0 0 0\nSelfIllum: false\nWire: false\nWireSize: 1\nOpacity: 1\n"
	    << "Map: " << (Part.material.empty() ? std::string{"none"} : Part.material) << "\nMaxDistance: 3000\nMinDistance: 0\n"
	    << "Transform:\n 1 0 0 0\n 0 1 0 0\n 0 0 1 0\n 0 0 0 1\n\n"
	    << "NumVerts: " << Part.points.size() << "\n";
	auto const normals{normals_of(Part)};
	for (std::size_t i = 0; i < Part.points.size(); ++i)
	{
		if (i % 3 == 0)
			Out << "-1\n"; // the normals of the three vertices follow, not a smoothing group
		auto const point{model_axes(Part.points[i] - Origin)};
		auto const normal{model_axes(normals[i])};
		Out << point.x << ' ' << point.y << ' ' << point.z << "  " << normal.x << ' ' << normal.y << ' ' << normal.z << "  " << Part.uvs[i].x << ' ' << Part.uvs[i].y << "\n";
	}
	Out << "\n";
}

} // namespace

part &model::add(std::string const &Name, std::string const &Material, glm::ivec3 const &Colour, bool const Smooth)
{
	if (auto const found{std::ranges::find(m_parts, Name, &part::name)}; found != m_parts.end())
		return *found;
	part made;
	made.name = Name;
	made.material = Material;
	made.colour = Colour;
	made.smooth = Smooth;
	m_parts.push_back(std::move(made));
	return m_parts.back();
}

std::size_t model::triangles() const
{
	std::size_t count{0};
	for (auto const &one : m_parts)
		count += one.points.size() / 3;
	return count;
}

std::string model::text(glm::dvec3 const &Origin) const
{
	std::ostringstream out;
	out.precision(4);
	out << std::fixed;
	out << "// a structure made by the editor: its parts are the submodels, the first one the parent of the others\n";
	std::string parent{"none"};
	for (auto const &one : m_parts)
	{
		if (one.points.empty())
			continue;
		write_part(out, one, parent, Origin);
		if (parent == "none")
			parent = one.name;
	}
	return out.str();
}

bool model::write(std::string const &Path, glm::dvec3 const &Origin) const
{
	std::ofstream file{Path};
	if (false == file.good())
		return false;
	file << text(Origin);
	return file.good();
}

void builder::triangle(glm::dvec3 const &A, glm::dvec3 const &B, glm::dvec3 const &C, glm::dvec3 const &Away) const
{
	if (into == nullptr || glm::length(glm::cross(B - A, C - A)) < 1e-9)
		return;
	auto const facing{glm::dot(normal_of(A, B, C), Away) >= 0.0};
	std::array<glm::dvec3, 3> const corners{A, facing ? B : C, facing ? C : B};
	auto const normal{normal_of(corners[0], corners[1], corners[2])};
	for (auto const &corner : corners)
	{
		into->points.push_back(corner);
		into->uvs.push_back(uv_of(corner, normal, tile));
	}
}

void builder::quad(glm::dvec3 const &A, glm::dvec3 const &B, glm::dvec3 const &C, glm::dvec3 const &D, glm::dvec3 const &Away) const
{
	triangle(A, B, C, Away);
	triangle(A, C, D, Away);
}

void builder::bar(glm::dvec3 const &From, glm::dvec3 const &To, double const Width, double const Height) const
{
	auto const axis{To - From};
	if (glm::length(axis) < 1e-6)
		return;
	auto const along{glm::normalize(axis)};
	// across: level, square to the bar; a bar standing upright has any level way across
	auto side{glm::cross(kUp, along)};
	side = glm::length(side) > 1e-6 ? glm::normalize(side) : glm::dvec3{1.0, 0.0, 0.0};
	auto const rise{glm::normalize(glm::cross(along, side))};
	auto const half{side * (0.5 * Width)};
	auto const high{rise * Height};
	std::array<glm::dvec3, 4> const start{From - half, From + half, From + half + high, From - half + high};
	std::array<glm::dvec3, 4> const end{To - half, To + half, To + half + high, To - half + high};
	quad(start[0], start[1], start[2], start[3], -along);
	quad(end[0], end[1], end[2], end[3], along);
	quad(start[0], end[0], end[1], start[1], -rise);
	quad(start[3], end[3], end[2], start[2], rise);
	quad(start[0], end[0], end[3], start[3], -side);
	quad(start[1], end[1], end[2], start[2], side);
}

void builder::solid(std::array<glm::dvec3, 8> const &Corners) const
{
	glm::dvec3 middle{0.0};
	for (auto const &corner : Corners)
		middle += corner * 0.125;
	auto const face = [&](int const A, int const B, int const C, int const D) {
		auto const centre{(Corners[A] + Corners[B] + Corners[C] + Corners[D]) * 0.25};
		quad(Corners[A], Corners[B], Corners[C], Corners[D], centre - middle);
	};
	face(0, 1, 2, 3);
	face(4, 5, 6, 7);
	for (int i = 0; i < 4; ++i)
		face(i, (i + 1) % 4, 4 + (i + 1) % 4, 4 + i);
}

void builder::member(glm::dvec3 const &From, glm::dvec3 const &To, double const Size) const
{
	auto const axis{To - From};
	if (glm::length(axis) < 1e-6)
		return;
	auto const along{glm::normalize(axis)};
	auto side{glm::cross(kUp, along)};
	side = glm::length(side) > 1e-6 ? glm::normalize(side) : glm::dvec3{1.0, 0.0, 0.0};
	// the bar grows from its line the way its section rises: moved back by half of it
	auto const back{glm::normalize(glm::cross(along, side)) * (0.5 * Size)};
	bar(From - back, To - back, Size, Size);
}

void builder::girder(glm::dvec3 const &From, glm::dvec3 const &To, double const Flange, double const Height) const
{
	auto const plate{std::min(0.06, 0.2 * Height)};
	glm::dvec3 const lift{0.0, Height - plate, 0.0};
	bar(From, To, Flange, plate);
	bar(From + glm::dvec3{0.0, plate, 0.0}, To + glm::dvec3{0.0, plate, 0.0}, 0.04, Height - 2.0 * plate);
	bar(From + lift, To + lift, Flange, plate);
}

void builder::railing(glm::dvec3 const &From, glm::dvec3 const &To, double const Height, double const Spacing) const
{
	auto const length{glm::distance(From, To)};
	if (length < 0.2)
		return;
	auto const posts{std::max(1, static_cast<int>(std::lround(length / std::max(0.5, Spacing))))};
	for (int i = 0; i <= posts; ++i)
	{
		auto const foot{glm::mix(From, To, static_cast<double>(i) / posts)};
		bar(foot, foot + kUp * Height, 0.06, 0.06);
	}
	bar(From + kUp * (Height - 0.06), To + kUp * (Height - 0.06), 0.07, 0.06);
	bar(From + kUp * (0.5 * Height), To + kUp * (0.5 * Height), 0.04, 0.04);
}

} // namespace structure
