# Unit tests of the simulation, built and run without the game data: the test vehicles are JSON
# definitions in data/ (see vehicle.h), loaded from memory by the game's own vehicle loader. The code
# under test is the game's core library (eu07_core), the very objects the game links; the rest of the
# engine it reaches for is replaced with stubs (engine_stubs.cpp). They run as part of the build,
# from the IDE, or with ctest.

set(UNIT_TESTS_NAME eu07_unit_tests)

# a new test_*.cpp file or file in data/ (definitions, real vehicles, golden traces) is picked up on its own
file(GLOB UNIT_TEST_CASES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")
file(GLOB_RECURSE UNIT_TEST_DATA CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/data/*")

add_executable(${UNIT_TESTS_NAME}
	main.cpp
	vehicle.cpp
	fiz.cpp
	drive.cpp
	consist.cpp
	scenario.cpp
	golden.cpp
	invariants.cpp
	figures.cpp
	engine_stubs.cpp
	editor_stubs.cpp
	# the geometry of the lines the track editor designs, standing on its own
	"${CMAKE_SOURCE_DIR}/editor/editorAlignment.cpp"
	# and the heights of the tracks laid
	"${CMAKE_SOURCE_DIR}/editor/editorGroundFollow.cpp"
	"${CMAKE_SOURCE_DIR}/editor/editorProfile.cpp"
	# and the switches laid into the curves
	"${CMAKE_SOURCE_DIR}/editor/editorSwitchCurve.cpp"
	# and the tracks designed from the lines of a railway map
	"${CMAKE_SOURCE_DIR}/editor/editorRailData.cpp"
	"${CMAKE_SOURCE_DIR}/editor/editorRailDesign.cpp"
	"${CMAKE_SOURCE_DIR}/editor/editorStructure.cpp"
	"${CMAKE_SOURCE_DIR}/editor/editorCrossing.cpp"
	"${CMAKE_SOURCE_DIR}/editor/editorScatter.cpp"
	"${CMAKE_SOURCE_DIR}/utilities/asciiname.cpp"
	# and what a change of the layout of a piece of a road means for the rest of the road
	"${CMAKE_SOURCE_DIR}/editor/editorRoadLayout.cpp"
	# and the safety barriers along the roads
	"${CMAKE_SOURCE_DIR}/world/RoadBarrier.cpp"
	# the snow lying, as the weather lays it, drifts and melts it
	"${CMAKE_SOURCE_DIR}/environment/snowcover.cpp"
	# and the weather made of the climate of a place
	"${CMAKE_SOURCE_DIR}/environment/climate.cpp"
	"${CMAKE_SOURCE_DIR}/environment/climatestations.cpp"
	${UNIT_TEST_CASES})

# the core library brings the game's build settings along
target_link_libraries(${UNIT_TESTS_NAME} PRIVATE eu07_core)
target_include_directories(${UNIT_TESTS_NAME} PRIVATE "${DEPS_DIR}/doctest")
# the names the tests CAPTURE are often C strings, shown as text rather than pointers
target_compile_definitions(${UNIT_TESTS_NAME} PRIVATE EU07_UNIT_TESTS_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data" DOCTEST_CONFIG_TREAT_CHAR_STAR_AS_STRING)
if (MSVC)
	target_compile_options(${UNIT_TESTS_NAME} PRIVATE /bigobj)
endif()
# in its build folder whatever the game is built into (CMAKE_RUNTIME_OUTPUT_DIRECTORY, a game folder at home): it needs no game data
set_target_properties(${UNIT_TESTS_NAME} PROPERTIES FOLDER "tests" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_test(NAME ${UNIT_TESTS_NAME} COMMAND ${UNIT_TESTS_NAME})

# for running the tests at the end of building the game, see the main CMakeLists.txt
set(UNIT_TEST_DATA ${UNIT_TEST_DATA} PARENT_SCOPE)
