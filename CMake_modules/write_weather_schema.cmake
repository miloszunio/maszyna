# Runs the tool that writes weather-schema.txt (McZapkie/weatherschema.h) in the folder of the game, as a step of its build:
#   cmake -DTOOL=<eu07_weather_schema> -DFOLDER=<the folder of the game> -P write_weather_schema.cmake
# the game doesn't read the file, so one that cannot be written is a warning and the build goes on
execute_process(
	COMMAND "${TOOL}"
	WORKING_DIRECTORY "${FOLDER}"
	RESULT_VARIABLE RESULT
	ERROR_VARIABLE MESSAGE)
if (NOT RESULT EQUAL 0)
	message(WARNING "weather-schema.txt was not written to ${FOLDER}: ${MESSAGE}")
endif()
