// A structure made by the editor written as one T3D model (editor/editorStructure): its parts as submodels, the faces turned
// outwards.

#include "stdafx.h"
#include "doctest.h"

#include <string>

#include "editor/editorStructure.hpp"

namespace
{

// triangles of the part not facing away from the point
int facing_inwards(structure::part const &Part, glm::dvec3 const &Inside)
{
	int count{0};
	for (std::size_t i = 0; i + 2 < Part.points.size(); i += 3)
	{
		auto const normal{glm::cross(Part.points[i + 1] - Part.points[i], Part.points[i + 2] - Part.points[i])};
		auto const middle{(Part.points[i] + Part.points[i + 1] + Part.points[i + 2]) / 3.0};
		if (glm::dot(normal, middle - Inside) <= 0.0)
			++count;
	}
	return count;
}

std::size_t count_of(std::string const &Text, std::string const &What)
{
	std::size_t count{0};
	for (auto at{Text.find(What)}; at != std::string::npos; at = Text.find(What, at + What.size()))
		++count;
	return count;
}

} // namespace

TEST_CASE("structure: a bar is a closed box seen from outside, whichever way it runs")
{
	structure::model model;
	auto &part{model.add("obiekt", "beton")};
	structure::builder const build{&part, 2.0};
	build.bar({0.0, 0.0, 0.0}, {4.0, 0.0, 0.0}, 1.0, 0.5);
	CHECK(part.points.size() == 36);
	CHECK(part.uvs.size() == 36);
	CHECK(facing_inwards(part, {2.0, 0.25, 0.0}) == 0);
	// upright, as a post
	auto &post{model.add("slupek", "")};
	structure::builder const posts{&post, 2.0};
	posts.bar({1.0, 0.0, 1.0}, {1.0, 2.0, 1.0}, 0.1, 0.1);
	CHECK(post.points.size() == 36);
	// a face asked to look one way does, however its corners are given
	auto &face{model.add("lico", "beton")};
	structure::builder const faces{&face, 2.0};
	faces.quad({0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 1.0}, {1.0, 0.0, 0.0}, {0.0, -1.0, 0.0});
	CHECK(facing_inwards(face, {0.5, 1.0, 0.5}) == 0);
}

TEST_CASE("structure: the model is one file of submodels, the first the parent of the others")
{
	structure::model model;
	structure::builder const body{&model.add("obiekt", "kamien"), 2.0};
	body.bar({0.0, 0.0, 0.0}, {4.0, 0.0, 0.0}, 1.0, 0.5);
	structure::builder const steel{&model.add("balustrady", "", {70, 78, 72}), 2.0};
	steel.railing({0.0, 0.5, 0.0}, {4.0, 0.5, 0.0}, 1.1, 2.0);
	model.add("pusty", "kamien"); // a part with nothing in it isn't written
	// asked for again, a part is the one made before
	CHECK(&model.add("obiekt", "inny") == &model.parts().front());
	auto const text{model.text({0.0, 0.0, 0.0})};
	CHECK(count_of(text, "Type: Mesh") == 2);
	CHECK(count_of(text, "Parent: none") == 1);
	CHECK(count_of(text, "Parent: obiekt") == 1);
	CHECK(count_of(text, "Map: kamien") == 1);
	CHECK(count_of(text, "Map: none") == 1);
	CHECK(count_of(text, "Diffuse: 70 78 72") == 1);
	CHECK(count_of(text, "NumVerts: 36\n") == 1);
	// a triangle each: the mask -1, then three vertices of a place, a normal and a place on the image
	CHECK(count_of(text, "\n-1\n") == model.triangles());
	// the railing: three posts and two rails
	CHECK(model.parts()[1].points.size() == 5 * 36);
}
