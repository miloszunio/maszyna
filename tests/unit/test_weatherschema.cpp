#include "stdafx.h"
#include "doctest.h"
#include "McZapkie/weatherschema.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

// the description of physics.weather.* for the starter (McZapkie/weatherschema.h): what it says is what the tables of the game hold

namespace {

using fields = std::vector<std::string_view>;

fields split(std::string_view Line, char const Separator = '\t') {
	fields parts;
	while (true) {
		auto const at{Line.find(Separator)};
		parts.push_back(Line.substr(0, at));
		if (at == std::string_view::npos)
			break;
		Line.remove_prefix(at + 1);
	}
	return parts;
}

float number(std::string_view const Text) {
	float value{0.f};
	auto const result{std::from_chars(Text.data(), Text.data() + Text.size(), value)};
	CHECK(result.ec == std::errc{});
	CHECK(result.ptr == Text.data() + Text.size());
	return value;
}

// the lines that carry an entry: the comment and the line of the schema number are left out
std::vector<fields> entries(std::string const &Text) {
	std::vector<fields> result;
	for (auto const &line : split(Text, '\n')) {
		if (line.empty() || line.front() == '#' || line.starts_with("schema\t"))
			continue;
		result.push_back(split(line));
	}
	return result;
}

bool is_identifier(std::string_view const Text) {
	return !Text.empty() && std::ranges::all_of(Text, [](char const Character) { return (Character >= 'a' && Character <= 'z') || (Character >= '0' && Character <= '9') || Character == '_'; });
}

} // namespace

TEST_CASE("weather schema: a comment, the schema number, then a line to a parameter of the tables, in their order") {
	auto const text{weather_physics::schema_text()};
	CHECK(text.starts_with('#'));
	CHECK(text.ends_with('\n'));
	CHECK(text.find('\r') == std::string::npos);
	CHECK(text.find("\nschema\t1\n") != std::string::npos);

	auto const lines{entries(text)};
	REQUIRE(lines.size() == weather_physics::bool_parameters.size() + weather_physics::float_parameters.size());
	// every entry of the tables, the switches first
	std::size_t at{0};
	for (auto const &parameter : weather_physics::bool_parameters) {
		auto const &line{lines[at++]};
		CAPTURE(parameter.key);
		REQUIRE(line.size() == 5);
		CHECK(line[0] == "bool");
		CHECK(line[1] == parameter.key);
		CHECK(line[3] == parameter.group);
		CHECK(line[4] == parameter.label);
	}
	for (auto const &parameter : weather_physics::float_parameters) {
		auto const &line{lines[at++]};
		CAPTURE(parameter.key);
		REQUIRE(line.size() == 8);
		CHECK(line[0] == "float");
		CHECK(line[1] == parameter.key);
		CHECK(line[6] == parameter.group);
		CHECK(line[7] == parameter.label);
	}
}

TEST_CASE("weather schema: the defaults, the ranges and the decimals are the ones of the code, the numbers written with a point") {
	weather_physics::settings const defaults{};
	auto const text{weather_physics::schema_text()};
	auto const lines{entries(text)};
	REQUIRE(lines.size() == weather_physics::bool_parameters.size() + weather_physics::float_parameters.size());

	std::size_t at{0};
	for (auto const &parameter : weather_physics::bool_parameters) {
		CAPTURE(parameter.key);
		CHECK(lines[at++][2] == ((defaults.*parameter.member) ? "1" : "0"));
	}
	for (auto const &parameter : weather_physics::float_parameters) {
		auto const &line{lines[at++]};
		CAPTURE(parameter.key);
		// what is written reads back as the very float of the table
		CHECK(number(line[2]) == defaults.*parameter.member);
		CHECK(number(line[3]) == parameter.minimum);
		CHECK(number(line[4]) == parameter.maximum);
		CHECK(number(line[5]) == static_cast<float>(weather_physics::schema_decimals(parameter.format)));
		for (auto const index : {std::size_t{2}, std::size_t{3}, std::size_t{4}})
			CHECK(line[index].find(',') == std::string_view::npos);
	}
}

TEST_CASE("weather schema: the tables hold what the starter needs of them") {
	weather_physics::settings const defaults{};
	std::set<std::string_view> keys;
	auto const check_common = [&keys](std::string_view const Key, char const *const Label, char const *const Group) {
		CAPTURE(Key);
		// only the keys of the weather: the starter writes nothing else out of this file
		CHECK((Key == "physics.weather" || Key.starts_with("physics.weather.")));
		CHECK(keys.insert(Key).second);
		REQUIRE(Label != nullptr);
		CHECK(std::string_view{Label}.find_first_of("\t\r\n") == std::string_view::npos);
		CHECK_FALSE(std::string_view{Label}.empty());
		REQUIRE(Group != nullptr);
		// the starter builds the translation keys of its sections out of the groups
		CHECK(is_identifier(Group));
	};

	for (auto const &parameter : weather_physics::bool_parameters)
		check_common(parameter.key, parameter.label, parameter.group);
	for (auto const &parameter : weather_physics::float_parameters) {
		check_common(parameter.key, parameter.label, parameter.group);
		CHECK(parameter.minimum < parameter.maximum);
		CHECK(defaults.*parameter.member >= parameter.minimum);
		CHECK(defaults.*parameter.member <= parameter.maximum);
		// a printf format of the table the decimals are read from, not the two they fall back to
		REQUIRE(parameter.format != nullptr);
		CHECK(std::string_view{parameter.format}.size() == 4);
		CHECK(std::string_view{parameter.format}.starts_with("%."));
		CHECK(std::string_view{parameter.format}.ends_with('f'));
	}
}

TEST_CASE("weather schema: the decimals of a format, and two where it is not one") {
	CHECK(weather_physics::schema_decimals("%.0f") == 0);
	CHECK(weather_physics::schema_decimals("%.2f") == 2);
	CHECK(weather_physics::schema_decimals("%.4f") == 4);
	CHECK(weather_physics::schema_decimals("") == 2);
	CHECK(weather_physics::schema_decimals("%d") == 2);
	CHECK(weather_physics::schema_decimals("%.xf") == 2);
}
