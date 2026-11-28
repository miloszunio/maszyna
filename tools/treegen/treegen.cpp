/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// eu07_treegen: trees of the generator written as text models (.t3d), to be put into the game and looked at before the renderer
// draws the trees itself.
//
//   eu07_treegen --species dab_szypulkowy --seed 7 --height 22 --season autumn --out dab.t3d
//   eu07_treegen --profile moj_gatunek.txt --seed 7 --out moj.t3d
//   eu07_treegen --gallery models/drzewa_generator     (every species the game knows, a few trees each, in every season)
//   eu07_treegen --list

#include <array>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "world/trees/treeexport.h"

namespace
{

struct options
{
	std::string species{"dab_szypulkowy"};
	std::string profile; // a file of a species, instead of one the game knows
	trees::instance tree;
	bool height_given{false};
	trees::season time{trees::season::summer};
	std::string out;
	std::string gallery;
	bool list{false};
};

std::optional<trees::season> season_of(std::string_view const Word)
{
	using enum trees::season;
	if (Word == "spring")
		return spring;
	if (Word == "summer")
		return summer;
	if (Word == "autumn")
		return autumn;
	if (Word == "winter")
		return winter;
	return std::nullopt;
}

std::optional<trees::form> form_of(std::string_view const Word)
{
	using enum trees::form;
	if (Word == "open")
		return open;
	if (Word == "edge")
		return edge;
	if (Word == "forest")
		return forest;
	return std::nullopt;
}

template <typename Number> bool number_of(std::string_view const Word, Number &Value)
{
	auto const result{std::from_chars(Word.data(), Word.data() + Word.size(), Value)};
	return result.ec == std::errc{} && result.ptr == Word.data() + Word.size();
}

// one option and its value into the options; false on a fault
bool read_option(std::string_view const Key, std::string_view const Value, options &Options)
{
	if (Key == "--species")
		Options.species = Value;
	else if (Key == "--profile")
		Options.profile = Value;
	else if (Key == "--out")
		Options.out = Value;
	else if (Key == "--gallery")
		Options.gallery = Value;
	else if (Key == "--seed")
		return number_of(Value, Options.tree.seed);
	else if (Key == "--height")
	{
		Options.height_given = number_of(Value, Options.tree.height);
		return Options.height_given;
	}
	else if (Key == "--vitality")
		return number_of(Value, Options.tree.vitality);
	else if (Key == "--trim")
		return number_of(Value, Options.tree.trim_height);
	else if (Key == "--season")
	{
		auto const time{season_of(Value)};
		if (time)
			Options.time = *time;
		return time.has_value();
	}
	else if (Key == "--form")
	{
		auto const grown{form_of(Value)};
		if (grown)
			Options.tree.grown = *grown;
		return grown.has_value();
	}
	else
		return false;
	return true;
}

std::optional<trees::species> species_of(options const &Options)
{
	if (Options.profile.empty())
	{
		auto found{trees::builtin_species(Options.species)};
		if (false == found.has_value())
			std::fprintf(stderr, "no species %s, see --list\n", Options.species.c_str());
		return found;
	}
	std::ifstream file{Options.profile, std::ios::binary};
	std::stringstream text;
	text << file.rdbuf();
	std::string error;
	auto read{trees::parse_species(text.str(), error)};
	if (false == read.has_value())
		std::fprintf(stderr, "%s: %s\n", Options.profile.c_str(), error.c_str());
	return read;
}

bool write_tree(trees::species const &Species, trees::instance const &Tree, trees::season const Time, std::filesystem::path const &Path)
{
	auto const skeleton{trees::grow(Species, Tree)};
	std::vector<trees::tree_mesh> levels;
	for (int level = 0; level < trees::levels; ++level)
		levels.push_back(trees::build_mesh(skeleton, Species, Tree, level));
	std::ofstream out{Path, std::ios::binary};
	if (false == out.good())
	{
		std::fprintf(stderr, "can't write %s\n", Path.string().c_str());
		return false;
	}
	trees::export_settings settings;
	settings.time = Time;
	trees::write_t3d(out, levels, Species, settings);
	std::printf("%s: %zu triangles near, %zu far\n", Path.string().c_str(), levels.front().triangles(), levels.back().triangles());
	return true;
}

// every species the game knows: three trees of different sizes, in every season
int write_gallery(std::filesystem::path const &Folder)
{
	constexpr std::array seasons{std::pair{trees::season::spring, "wiosna"}, std::pair{trees::season::summer, "lato"}, std::pair{trees::season::autumn, "jesien"},
	                             std::pair{trees::season::winter, "zima"}};
	std::filesystem::create_directories(Folder);
	auto failed{0};
	for (auto const &id : trees::builtin_species())
	{
		auto const species{*trees::builtin_species(id)};
		// the first tree in every season, two more of other sizes in the summer
		for (int tree = 0; tree < 3; ++tree)
		{
			trees::instance made;
			made.seed = static_cast<std::uint32_t>(101 + tree * 7919);
			made.height = species.height_min + (species.height_max - species.height_min) * (0.25f + 0.3f * static_cast<float>(tree));
			for (auto const &[time, name] : seasons)
			{
				if (tree > 0 && time != trees::season::summer)
					continue;
				if (false == write_tree(species, made, time, Folder / std::format("{}_{}_{}.t3d", id, tree + 1, name)))
					++failed;
			}
		}
	}
	return failed == 0 ? 0 : 1;
}

} // namespace

int main(int const Count, char const *const *const Arguments)
{
	options settings;
	std::vector<std::string_view> const words(Arguments + 1, Arguments + Count);
	std::size_t index{0};
	while (index < words.size())
	{
		auto const key{words[index]};
		if (key == "--list")
		{
			settings.list = true;
			++index;
			continue;
		}
		if (index + 1 >= words.size() || false == read_option(key, words[index + 1], settings))
		{
			std::fprintf(stderr, "bad option %.*s, see the head of tools/treegen/treegen.cpp\n", static_cast<int>(key.size()), key.data());
			return 2;
		}
		index += 2;
	}
	if (settings.list)
	{
		for (auto const &id : trees::builtin_species())
			std::printf("%s\n", id.c_str());
		return 0;
	}
	if (false == settings.gallery.empty())
		return write_gallery(settings.gallery);
	auto const species{species_of(settings)};
	if (false == species.has_value())
		return 1;
	if (false == settings.height_given)
		settings.tree.height = 0.5f * (species->height_min + species->height_max);
	auto const out{settings.out.empty() ? std::format("{}_{}.t3d", species->id, settings.tree.seed) : settings.out};
	return write_tree(*species, settings.tree, settings.time, out) ? 0 : 1;
}
