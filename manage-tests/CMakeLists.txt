cmake_minimum_required(VERSION 3.11)

project(manage-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MT_LIBS REQUIRED gtkmm-4.0)

find_package(Threads REQUIRED)

add_executable(manage-tests
        main.cpp
        main_window.cpp
        main_window.h
        test_model.cpp
        test_model.h
        docker_render.cpp
        docker_render.h
        chrome_render.cpp
        chrome_render.h
        util.h
)

target_include_directories(manage-tests PRIVATE ${MT_LIBS_INCLUDE_DIRS})
target_link_options(manage-tests PRIVATE ${MT_LIBS_LDFLAGS})
target_link_libraries(manage-tests PRIVATE ${MT_LIBS_LIBRARIES} Threads::Threads)

# Absolute path to the litehtml-tests repository root (parent of this folder).
# Used at runtime to locate the Dockerfile, the check-with-chrome scripts and fonts/.
get_filename_component(MANAGE_TESTS_REPO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
target_compile_definitions(manage-tests PRIVATE MANAGE_TESTS_REPO="${MANAGE_TESTS_REPO_DIR}")

set_target_properties(manage-tests PROPERTIES
        CXX_STANDARD 17
        C_STANDARD 99
)
