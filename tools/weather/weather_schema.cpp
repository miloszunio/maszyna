/*
This Source Code Form is subject to the
terms of the Mozilla Public License, v.
2.0. If a copy of the MPL was not
distributed with this file, You can
obtain one at
http://mozilla.org/MPL/2.0/.
*/

// writes weather-schema.txt (McZapkie/weatherschema.h) into the working directory: the build of the game runs it in the folder of the
// simulator once the simulator is linked. no arguments, so that no path goes through the command line, where a name with a letter of
// the national code page of a user would not survive. the file is written under another name and renamed, so a reader never meets half of it.
// the exit code says whether it worked; the build takes a failure for a warning (CMake_modules/write_weather_schema.cmake), as the game
// doesn't need the file

#include "McZapkie/weatherschema.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

int main()
{
	std::filesystem::path const target{std::string{weather_physics::schema_file_name}};
	auto temporary{target};
	temporary += ".tmp";

	{
		std::ofstream file{temporary, std::ios::binary | std::ios::trunc};
		file << weather_physics::schema_text();
		file.flush();
		if (!file)
		{
			std::cerr << "weather_schema: cannot write " << temporary.string() << '\n';
			return 1;
		}
	}

	std::error_code error;
	std::filesystem::rename(temporary, target, error);
	if (error)
	{
		std::cerr << "weather_schema: cannot rename " << temporary.string() << " to " << target.string() << ": " << error.message() << '\n';
		std::filesystem::remove(temporary, error);
		return 1;
	}
	return 0;
}
