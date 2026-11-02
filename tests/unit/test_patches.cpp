#include "stdafx.h"
#include "doctest.h"
#include "scene/heightmapterrain.h"

#include <cstdio>
#include <fstream>
#include <string>

TEST_CASE("patches: the painted surfaces keep their transition and noise through the patches file") {
	heightmap::surface_patch painted;
	painted.name = "L132 t1 torowisko km 012.300-012.400";
	painted.material = "1435mm/tpbps-new2";
	painted.follow = true;
	painted.paint = true;
	painted.feather = 0.5f;
	painted.noise = 0.3f;
	painted.noise_scale = 4.0f;
	painted.outline = {{0.0, 0.0}, {100.0, 0.0}, {100.0, 7.0}, {0.0, 7.0}};
	heightmap::surface_patch mesh{painted};
	mesh.name = "surface 1";
	mesh.paint = false;
	mesh.lift = 0.03f;

	auto const path{std::string{"test_patches.txt"}};
	REQUIRE(heightmap::save_patches(path, {painted, mesh}));
	std::vector<heightmap::surface_patch> read;
	REQUIRE(heightmap::load_patches(path, read));
	REQUIRE(read.size() == 2);
	CHECK(read[0].name == painted.name);
	CHECK(read[0].paint);
	CHECK(read[0].feather == doctest::Approx(0.5f));
	CHECK(read[0].noise == doctest::Approx(0.3f));
	CHECK(read[0].noise_scale == doctest::Approx(4.0f));
	CHECK(read[0].outline.size() == 4);
	CHECK_FALSE(read[1].paint);
	CHECK(read[1].lift == doctest::Approx(0.03f));
	std::remove(path.c_str());
}

TEST_CASE("patches: the files written before the painting came are read as surfaces with a mesh") {
	auto const path{std::string{"test_patches_old.txt"}};
	{
		std::ofstream file{path};
		file << "# old\n";
		file << "L274 nasyp L km 000.000-000.100\tgrass\t0\t0\t0.02\t2\t0.00 0.00 100.00 0.00 100.00 5.00\n";
	}
	std::vector<heightmap::surface_patch> read;
	REQUIRE(heightmap::load_patches(path, read));
	REQUIRE(read.size() == 1);
	CHECK(read[0].follow);
	CHECK_FALSE(read[0].paint);
	CHECK(read[0].material == "grass");
	CHECK(read[0].outline.size() == 3);
	std::remove(path.c_str());
}
