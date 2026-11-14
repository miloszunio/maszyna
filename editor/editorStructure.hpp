/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// A structure made by the editor (a viaduct, a bridge, a culvert) as one model of the simulator in its text format, T3D:
// a file of submodels, each "Parent: Type: Name: Anim: Ambient: Diffuse: Specular: SelfIllum: Wire: WireSize: Opacity: Map:
// MaxDistance: MinDistance: Transform: NumVerts:" and its triangles, as TSubModel::Load reads them. The parts of the
// structure (the abutments, the span, the faces, the girders, the railings) are the submodels: one textured with the
// material chosen, one of plain colour (Map: none) for the steel. The triangles are one-sided, counter-clockwise seen from
// outside, each with its smoothing mask -1 and the normals of its vertices given.
// The model has the axes of the models (x the other way, the height third); the points are given here in the ones of the
// scenery and moved by the writer.

#include <array>
#include <deque>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace structure
{

struct part
{
	std::string name;
	std::string material; // empty: no texture, the colour alone
	glm::ivec3 colour{255, 255, 255};
	bool smooth{false}; // the normals shared by the triangles meeting at a point: a vault
	std::vector<glm::dvec3> points; // three a triangle
	std::vector<glm::vec2> uvs;
};

class model
{
  public:
	// the part of that name, made if there's none; the first one made is the parent of the others
	part &add(std::string const &Name, std::string const &Material, glm::ivec3 const &Colour = {255, 255, 255}, bool Smooth = false);
	std::deque<part> const &parts() const { return m_parts; }
	std::size_t triangles() const;
	// the text of the model, the points given from Origin
	std::string text(glm::dvec3 const &Origin) const;
	bool write(std::string const &Path, glm::dvec3 const &Origin) const;

  private:
	std::deque<part> m_parts; // a deque: the parts handed out stay where they are as more are made
};

// puts faces into a part, each turned to face Away, textured by where it lies: the image repeats every Tile metres
struct builder
{
	part *into{nullptr};
	double tile{2.0};

	// a triangle, a quad: seen from where Away points
	void triangle(glm::dvec3 const &A, glm::dvec3 const &B, glm::dvec3 const &C, glm::dvec3 const &Away) const;
	void quad(glm::dvec3 const &A, glm::dvec3 const &B, glm::dvec3 const &C, glm::dvec3 const &D, glm::dvec3 const &Away) const;
	// a block of eight corners, the four of its bottom and the four of its top over them, each face turned out of its middle
	void solid(std::array<glm::dvec3, 8> const &Corners) const;
	// a bar of a rectangular section from From to To: Width across, Height up from the line
	void bar(glm::dvec3 const &From, glm::dvec3 const &To, double Width, double Height) const;
	// a member of a truss, of a square section Size wide, the line its middle; it may stand or lean any way
	void member(glm::dvec3 const &From, glm::dvec3 const &To, double Size) const;
	// a girder of an I section from From to To, the line its bottom: flanges Flange wide, Height high
	void girder(glm::dvec3 const &From, glm::dvec3 const &To, double Flange, double Height) const;
	// a railing along the line: posts every Spacing or so, a handrail Height over the line and a rail halfway
	void railing(glm::dvec3 const &From, glm::dvec3 const &To, double Height, double Spacing) const;
};

} // namespace structure
