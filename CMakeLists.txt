cmake_minimum_required(VERSION 3.20)

project(litehtml-test LANGUAGES C CXX)

if (POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)
endif ()

set(LITEHTML_PATH "" CACHE PATH "Path to a compatible litehtml source checkout")
if (NOT LITEHTML_PATH OR NOT EXISTS "${LITEHTML_PATH}/CMakeLists.txt")
	message(FATAL_ERROR "LITEHTML_PATH must point to a litehtml source checkout containing CMakeLists.txt")
endif ()

option(LITEHTML_BUILD_RENDER2PNG "Build the render2png executable" OFF)
# The tests need the renderer library even when the executable is disabled.
set(_build_render2png_executable "${LITEHTML_BUILD_RENDER2PNG}")
set(LITEHTML_BUILD_RENDER2PNG ON)
add_subdirectory("${LITEHTML_PATH}" litehtml)
set(LITEHTML_BUILD_RENDER2PNG "${_build_render2png_executable}")
unset(_build_render2png_executable)

if (NOT TARGET litehtml_render2png)
	message(FATAL_ERROR
		"LITEHTML_PATH must point to a litehtml checkout that provides the litehtml_render2png target; "
		"ensure LITEHTML_BUILD_TESTING is OFF")
endif ()

set(TESTS_SRC
		encodings_test.cpp
#	    codepoint_test.cpp
#	    tstring_view_test.cpp
#	    url_test.cpp
#	    url_path_test.cpp
		render_test.cpp
		fonts.h
		fonts.cpp
)

# additional warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Tests

if (LITEHTML_BUILD_RENDER2PNG)
	add_executable(
			render2png
			render2png_main.cpp
			fonts.h
			fonts.cpp
	)

	set_target_properties(render2png PROPERTIES
			CXX_STANDARD 17
			C_STANDARD 99
	)

	target_link_libraries(render2png PRIVATE litehtml_render2png)
endif ()

option(EXTERNAL_GTEST "Use external GoogleTest instead of fetching from GitHub" OFF)

if (EXTERNAL_GTEST)
	find_package(Threads REQUIRED)
else()
	include(FetchContent)
	FetchContent_Declare(
	  googletest
	  URL https://github.com/google/googletest/releases/download/v1.16.0/googletest-1.16.0.tar.gz
	)
	# For Windows: Prevent overriding the parent project's compiler/linker settings
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	FetchContent_MakeAvailable(googletest)
endif()

enable_testing()

add_executable(
		${PROJECT_NAME}
		${TESTS_SRC}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	CXX_STANDARD 17
	C_STANDARD 99
)

target_include_directories(
		${PROJECT_NAME}
		PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
		${PROJECT_NAME}
		PRIVATE gtest_main gtest litehtml_render2png
)

if (EXTERNAL_GTEST)
	target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

if (LITEHTML_BUILD_MANAGE_FAILED)
	add_subdirectory(manage_failed manage_failed)
endif()

option(LITEHTML_BUILD_MANAGE_TESTS "Build the manage-tests GUI triage tool" OFF)

if (LITEHTML_BUILD_MANAGE_TESTS)
	add_subdirectory(manage-tests manage-tests)
endif()
