/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

#pragma once

// the description of physics.weather.* for the starter: weather-schema.txt, written next to the simulator at the end of its build
// (tools/weather/weather_schema.cpp) out of the very tables the game reads the keys with (McZapkie/weatherphysics.h), so what the starter
// shows, the key, the default, the range and the group, is what this build takes, with nothing to keep by hand. the starter shows each
// entry as a row of its settings and writes to eu07.ini only the keys the player moved
//
// the simulator never reads the file: it has the same defaults in the code and clamps every number to its range as it reads it, so a
// build without the file, or an eu07.ini written by hand, works as it did. a new parameter is a row in the table and shows in the
// starter on its own, in English until the starter has the text of it in its language
//
// plain text, a line to an entry, the fields divided by a tab; the label, last, is English text with no tab in it:
//   # a comment
//   schema<TAB>1
//   bool<TAB>key<TAB>default, 0 or 1<TAB>group<TAB>label
//   float<TAB>key<TAB>default<TAB>minimum<TAB>maximum<TAB>decimals<TAB>group<TAB>label
// the numbers are written as std::to_chars gives them, with a point, whatever the locale of the machine. a reader skips a line it doesn't
// understand, and the whole file when its schema number is not one it knows. a field added later goes to the end of its line; a change
// of what a field means raises the schema number

#include "McZapkie/weatherphysics.h"

#include <array>
#include <charconv>
#include <string>
#include <string_view>

namespace weather_physics
{

inline constexpr int schema_version{1};
inline constexpr std::string_view schema_file_name{"weather-schema.txt"};

// the digits after the point the starter shows a number with, out of the printf format of the table ("%.2f": 2); two where it isn't one
constexpr int schema_decimals(std::string_view const Format) noexcept
{
	if (Format.size() >= 4 && Format[0] == '%' && Format[1] == '.' && Format[2] >= '0' && Format[2] <= '9' && Format[3] == 'f')
		return Format[2] - '0';
	return 2;
}

namespace schema_detail
{

// a text of the table, which may be a pointer left null
constexpr std::string_view view(char const *const Text) noexcept
{
	return Text != nullptr ? std::string_view{Text} : std::string_view{};
}

// a text of the table, with the characters that would break the line or the field made spaces
inline void add_text(std::string &Output, std::string_view const Text)
{
	for (char const character : Text)
		Output += (character == '\t' || character == '\n' || character == '\r') ? ' ' : character;
}

inline void add_number(std::string &Output, float const Value)
{
	std::array<char, 32> buffer{};
	auto const result{std::to_chars(buffer.data(), buffer.data() + buffer.size(), Value)};
	Output.append(buffer.data(), result.ptr);
}

} // namespace schema_detail

// the whole file
inline std::string schema_text()
{
	settings const defaults{};
	std::string text{"# the settings of the weather in the physics, written by the build of the game out of its own tables: not to be edited\n"};
	text += "schema\t";
	text += std::to_string(schema_version);
	text += '\n';

	for (auto const &parameter : bool_parameters)
	{
		text += "bool\t";
		schema_detail::add_text(text, parameter.key);
		text += (defaults.*parameter.member) ? "\t1\t" : "\t0\t";
		schema_detail::add_text(text, schema_detail::view(parameter.group));
		text += '\t';
		schema_detail::add_text(text, schema_detail::view(parameter.label));
		text += '\n';
	}

	for (auto const &parameter : float_parameters)
	{
		text += "float\t";
		schema_detail::add_text(text, parameter.key);
		text += '\t';
		schema_detail::add_number(text, defaults.*parameter.member);
		text += '\t';
		schema_detail::add_number(text, parameter.minimum);
		text += '\t';
		schema_detail::add_number(text, parameter.maximum);
		text += '\t';
		text += std::to_string(schema_decimals(schema_detail::view(parameter.format)));
		text += '\t';
		schema_detail::add_text(text, schema_detail::view(parameter.group));
		text += '\t';
		schema_detail::add_text(text, schema_detail::view(parameter.label));
		text += '\n';
	}

	return text;
}

} // namespace weather_physics
