cmake_minimum_required(VERSION 3.11)

project(litehtml-test LANGUAGES C CXX)

add_subdirectory(${LITEHTML_PATH} litehtml)

set(TESTS_SRC
		encodings_test.cpp
#	    codepoint_test.cpp
#	    tstring_view_test.cpp
#	    url_test.cpp
#	    url_path_test.cpp
		render_test.cpp
		fonts.h
		fonts.cpp
		${LITEHTML_PATH}/containers/cairo/cairo_images_cache.h
		${LITEHTML_PATH}/containers/cairo/cairo_borders.cpp
		${LITEHTML_PATH}/containers/cairo/cairo_borders.h
		${LITEHTML_PATH}/containers/cairo/container_cairo.cpp
		${LITEHTML_PATH}/containers/cairo/container_cairo.h
		${LITEHTML_PATH}/containers/cairo/container_cairo_pango.cpp
		${LITEHTML_PATH}/containers/cairo/container_cairo_pango.h
		${LITEHTML_PATH}/containers/cairo/render2png.cpp
		${LITEHTML_PATH}/containers/cairo/render2png.h
)

# additional warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Tests

if (LITEHTML_BUILD_RENDER2PNG)
	set(RENDER2PNG_NAME render2png)

	find_package(PkgConfig REQUIRED)

	pkg_check_modules(RENDER2PNG_LIBS REQUIRED gdk-3.0 cairo pango pangocairo fontconfig)

	add_executable(
			${RENDER2PNG_NAME}
			render2png_main.cpp
			${LITEHTML_PATH}/containers/cairo/cairo_borders.cpp
			${LITEHTML_PATH}/containers/cairo/cairo_borders.h
			${LITEHTML_PATH}/containers/cairo/container_cairo.cpp
			${LITEHTML_PATH}/containers/cairo/container_cairo.h
			${LITEHTML_PATH}/containers/cairo/container_cairo_pango.cpp
			${LITEHTML_PATH}/containers/cairo/container_cairo_pango.h
			${LITEHTML_PATH}/containers/cairo/render2png.cpp
			${LITEHTML_PATH}/containers/cairo/cairo_images_cache.h
			${LITEHTML_PATH}/test/fonts.h
			${LITEHTML_PATH}/test/fonts.cpp
	)

	set_target_properties(${RENDER2PNG_NAME} PROPERTIES
			CXX_STANDARD 17
			C_STANDARD 99
			PUBLIC_HEADER "${HEADER_LITEHTML}"
	)

	target_include_directories(
			${RENDER2PNG_NAME}
			PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/containers/cairo
			PRIVATE ${RENDER2PNG_LIBS_INCLUDE_DIRS}
	)

	target_link_options(${RENDER2PNG_NAME} PRIVATE ${RENDER2PNG_LIBS_LDFLAGS})
	target_link_libraries(${RENDER2PNG_NAME} PRIVATE ${PROJECT_NAME} ${RENDER2PNG_LIBS_LIBRARIES})

endif ()

option(EXTERNAL_GTEST "Use external GoogleTest instead of fetching from GitHub" OFF)

if (EXTERNAL_GTEST)
	link_libraries("-Wl,--copy-dt-needed-entries")
else()
	include(FetchContent)
	FetchContent_Declare(
	  googletest
	  URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
	)
	# For Windows: Prevent overriding the parent project's compiler/linker settings
	set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
	FetchContent_GetProperties(googletest)
	if(NOT googletest_POPULATED)
	  FetchContent_Populate(googletest)
	  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
	endif()
endif()

enable_testing()

find_package(PkgConfig REQUIRED)
pkg_check_modules(TEST_LIBS REQUIRED gdk-3.0 cairo pango pangocairo fontconfig)

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
		PRIVATE ${TEST_LIBS_INCLUDE_DIRS}
		PRIVATE ${LITEHTML_PATH}/include
		PRIVATE ${LITEHTML_PATH}/src
		PRIVATE ${LITEHTML_PATH}/containers/cairo
)

target_link_options(${PROJECT_NAME} PRIVATE ${TEST_LIBS_LDFLAGS} -Llitehtml)
target_link_libraries(
		${PROJECT_NAME}
		gtest_main
		${TEST_LIBS_LIBRARIES}
		litehtml
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
