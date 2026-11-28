/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#include "world/trees/treespecies.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <utility>

namespace trees
{

namespace
{

// the words of one line, a quoted word kept whole
std::vector<std::string_view> words_of(std::string_view Line)
{
	std::vector<std::string_view> words;
	std::size_t position{0};
	while (position < Line.size())
	{
		auto const character{Line[position]};
		if (character == ' ' || character == '\t' || character == '\r')
		{
			++position;
			continue;
		}
		if (character == '#')
			break;
		if (character == '"')
		{
			auto const end{Line.find('"', position + 1)};
			auto const stop{end == std::string_view::npos ? Line.size() : end};
			words.push_back(Line.substr(position + 1, stop - position - 1));
			position = stop + 1;
			continue;
		}
		auto const end{Line.find_first_of(" \t\r#", position)};
		auto const stop{end == std::string_view::npos ? Line.size() : end};
		words.push_back(Line.substr(position, stop - position));
		position = stop;
	}
	return words;
}

bool number_of(std::string_view const Word, float &Value)
{
	auto const result{std::from_chars(Word.data(), Word.data() + Word.size(), Value)};
	return result.ec == std::errc{} && result.ptr == Word.data() + Word.size();
}

bool number_of(std::string_view const Word, int &Value)
{
	auto const result{std::from_chars(Word.data(), Word.data() + Word.size(), Value)};
	return result.ec == std::errc{} && result.ptr == Word.data() + Word.size();
}

template <typename Enum, std::size_t Count> bool choice_of(std::string_view const Word, std::array<std::pair<std::string_view, Enum>, Count> const &Choices, Enum &Value)
{
	auto const found{std::ranges::find(Choices, Word, &std::pair<std::string_view, Enum>::first)};
	if (found == Choices.end())
		return false;
	Value = found->second;
	return true;
}

constexpr std::array growths{std::pair{std::string_view{"colonisation"}, growth::colonisation}, std::pair{std::string_view{"whorls"}, growth::whorls}};

constexpr std::array crowns{std::pair{std::string_view{"ellipsoid"}, crown::ellipsoid}, std::pair{std::string_view{"round"}, crown::round},
                            std::pair{std::string_view{"cone"}, crown::cone},           std::pair{std::string_view{"column"}, crown::column},
                            std::pair{std::string_view{"umbrella"}, crown::umbrella},   std::pair{std::string_view{"weeping"}, crown::weeping}};

constexpr std::array foliages{std::pair{std::string_view{"oval"}, foliage::oval},
                              std::pair{std::string_view{"lobed"}, foliage::lobed},
                              std::pair{std::string_view{"palmate"}, foliage::palmate},
                              std::pair{std::string_view{"heart"}, foliage::heart},
                              std::pair{std::string_view{"pinnate"}, foliage::pinnate},
                              std::pair{std::string_view{"narrow"}, foliage::narrow},
                              std::pair{std::string_view{"needle_tuft"}, foliage::needle_tuft},
                              std::pair{std::string_view{"needle_brush"}, foliage::needle_brush},
                              std::pair{std::string_view{"needle_flat"}, foliage::needle_flat}};

// the keys of one number
using number_key = std::pair<std::string_view, float species::*>;
constexpr std::array numbers{number_key{"stem_spread", &species::stem_spread},
                             number_key{"crown_base", &species::crown_base},
                             number_key{"crown_width", &species::crown_width},
                             number_key{"trunk_radius", &species::trunk_radius},
                             number_key{"trunk_wobble", &species::trunk_wobble},
                             number_key{"trunk_lean", &species::trunk_lean},
                             number_key{"attractors", &species::attractors},
                             number_key{"influence", &species::influence},
                             number_key{"kill", &species::kill},
                             number_key{"tropism_up", &species::tropism_up},
                             number_key{"droop", &species::droop},
                             number_key{"wander", &species::wander},
                             number_key{"clumps", &species::clumps},
                             number_key{"twigs", &species::twigs},
                             number_key{"twig_length", &species::twig_length},
                             number_key{"hang", &species::hang},
                             number_key{"whorl_step", &species::whorl_step},
                             number_key{"whorl_branches", &species::whorl_branches},
                             number_key{"branch_angle", &species::branch_angle},
                             number_key{"branch_droop", &species::branch_droop},
                             number_key{"branch_upturn", &species::branch_upturn},
                             number_key{"side_branches", &species::side_branches},
                             number_key{"leaf_size", &species::leaf_size},
                             number_key{"leaf_density", &species::leaf_density},
                             number_key{"leaf_budget", &species::leaf_budget},
                             number_key{"twig_radius", &species::twig_radius},
                             number_key{"flower_size", &species::flower_size},
                             number_key{"fruit_size", &species::fruit_size},
                             number_key{"bud_day", &species::bud_day},
                             number_key{"flower_day", &species::flower_day},
                             number_key{"full_leaf_day", &species::full_leaf_day},
                             number_key{"fruit_day", &species::fruit_day},
                             number_key{"colour_day", &species::colour_day},
                             number_key{"fall_day", &species::fall_day},
                             number_key{"stiffness", &species::stiffness}};

// the keys of a colour
using colour_key = std::pair<std::string_view, glm::ivec3 species::*>;
constexpr std::array colours{colour_key{"bark", &species::bark},
                             colour_key{"leaf_summer", &species::leaf_summer},
                             colour_key{"leaf_spring", &species::leaf_spring},
                             colour_key{"leaf_autumn", &species::leaf_autumn},
                             colour_key{"flower", &species::flower},
                             colour_key{"fruit", &species::fruit}};

// the keys of a flag
using flag_key = std::pair<std::string_view, bool species::*>;
constexpr std::array flags{flag_key{"evergreen", &species::evergreen}, flag_key{"keeps_dry_leaves", &species::keeps_dry_leaves}};

// one line read into the profile; the fault, or an empty text
std::string read_line(std::vector<std::string_view> const &Words, species &Species)
{
	auto const key{Words.front()};
	auto const count{Words.size() - 1};
	if (key == "id" || key == "name")
	{
		if (count != 1)
			return std::format("{} takes one word", key);
		(key == "id" ? Species.id : Species.name) = std::string{Words[1]};
		return {};
	}
	if (key == "growth")
		return count == 1 && choice_of(Words[1], growths, Species.kind) ? std::string{} : std::string{"growth is colonisation or whorls"};
	if (key == "crown")
		return count == 1 && choice_of(Words[1], crowns, Species.shape) ? std::string{} : std::string{"unknown crown"};
	if (key == "leaves")
		return count == 1 && choice_of(Words[1], foliages, Species.leaves) ? std::string{} : std::string{"unknown leaves"};
	if (key == "height")
		return count == 2 && number_of(Words[1], Species.height_min) && number_of(Words[2], Species.height_max) && Species.height_min > 0.f && Species.height_min <= Species.height_max ?
		           std::string{} :
		           std::string{"height takes the least and the most, metres"};
	if (key == "stems")
		return count == 2 && number_of(Words[1], Species.stems_min) && number_of(Words[2], Species.stems_max) && Species.stems_min >= 1 && Species.stems_min <= Species.stems_max &&
		               Species.stems_max <= 12 ?
		           std::string{} :
		           std::string{"stems takes the least and the most, 1..12"};
	if (auto const number{std::ranges::find(numbers, key, &number_key::first)}; number != numbers.end())
		return count == 1 && number_of(Words[1], Species.*(number->second)) ? std::string{} : std::format("{} takes one number", key);
	if (auto const colour{std::ranges::find(colours, key, &colour_key::first)}; colour != colours.end())
	{
		auto &value{Species.*(colour->second)};
		return count == 3 && number_of(Words[1], value.r) && number_of(Words[2], value.g) && number_of(Words[3], value.b) ? std::string{} : std::format("{} takes red, green and blue", key);
	}
	if (auto const flag{std::ranges::find(flags, key, &flag_key::first)}; flag != flags.end())
	{
		int value{0};
		if (count != 1 || false == number_of(Words[1], value))
			return std::format("{} takes 0 or 1", key);
		Species.*(flag->second) = value != 0;
		return {};
	}
	return std::format("unknown key {}", key);
}

} // namespace

std::optional<species> parse_species(std::string_view Text, std::string &Error)
{
	species result;
	int number{0};
	while (false == Text.empty())
	{
		++number;
		auto const end{Text.find('\n')};
		auto const line{Text.substr(0, end)};
		Text = end == std::string_view::npos ? std::string_view{} : Text.substr(end + 1);
		auto const words{words_of(line)};
		if (words.empty())
			continue;
		if (auto fault{read_line(words, result)}; false == fault.empty())
		{
			Error = std::format("line {}: {}", number, fault);
			return std::nullopt;
		}
	}
	if (result.id.empty())
	{
		Error = "the profile has no id";
		return std::nullopt;
	}
	if (result.name.empty())
		result.name = result.id;
	return result;
}

std::optional<species> builtin_species(std::string_view const Id)
{
	auto const text{builtin_species_text(Id)};
	if (text.empty())
		return std::nullopt;
	std::string error;
	return parse_species(text, error);
}

} // namespace trees
