#
# Copyright (C) 2016-2017 IRIE Shinsuke
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
cmake_minimum_required(VERSION 3.0)

project(smaa-cpp
	VERSION 0.3.0
	LANGUAGES CXX
)

set(PROJECT_SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

# Options
option(WITH_SUBPIXEL_RENDERING "Enable subpixel rendering"             ON)
option(WITH_LIBRARY_STATIC     "Enable building static library"        ON)
option(WITH_LIBRARY_SHARED     "Enable building shared library"        OFF)
option(WITH_EXAMPLE            "Enable building sample program"        ON)
option(WITH_TESTS              "Enable tests running sample program"   ON)
option(WITH_INSTALL_HEADERS    "Force installation of library headers" OFF)
option(WITH_INSTALL_BIN        "Install command line programs"         OFF)

option(WITH_EXAMPLE_PREFER_SHLIB "Sample program prefers linking shared library" OFF)
mark_as_advanced(WITH_EXAMPLE_PREFER_SHLIB)

# Dependencies
if(WITH_EXAMPLE)
	find_package(PNG)
	if(PNG_FOUND)
		if(NOT WITH_LIBRARY_SHARED AND NOT WITH_LIBRARY_STATIC)
			set(WITH_LIBRARY_STATIC ON)
		endif()
	else()
		message(WARNING "PNG library not found, disabling WITH_EXAMPLE")
		set(WITH_EXAMPLE OFF)
	endif()
endif()

if(WITH_TESTS AND NOT WITH_EXAMPLE)
	message(WARNING "Running tests needs WITH_EXAMPLE=ON, disabling WITH_TESTS")
	set(WITH_TESTS OFF)
endif()

if(WITH_LIBRARY_STATIC)
	set(WITH_INSTALL_HEADERS ON)
endif()

# Subdirectories
add_subdirectory(bin)

if(WITH_LIBRARY_STATIC OR WITH_LIBRARY_SHARED)
	add_subdirectory(lib)
	add_subdirectory(include)
endif()

# Tests
if (WITH_TESTS)
	enable_testing()
	add_subdirectory(tests)
	set(WITH_EXAMPLE ON)
endif()

# Packaging
#  Don't use ZIP generator if including shared library for Linux,
#  otherwise symlinks will be lost.
set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}_${PROJECT_VERSION}_${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})
include(CPack)
