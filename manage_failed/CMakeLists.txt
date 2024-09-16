cmake_minimum_required(VERSION 3.11)
project(manage_failed)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LB_LIBS REQUIRED gdkmm-3.0 gtkmm-3.0 libcurl cairo pango pangocairo)

add_executable(manage_failed main.cpp
		main_window.cpp
		main_window.h
		config_dialog.cpp
		config_dialog.h)

include_directories(manage_failed ${LB_LIBS_INCLUDE_DIRS})
target_link_options(manage_failed PRIVATE ${LB_LIBS_LDFLAGS})

set_target_properties(manage_failed PROPERTIES
		CXX_STANDARD 17
		C_STANDARD 99
)
