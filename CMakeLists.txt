#
# Copyright (C) 2016 IRIE Shinsuke
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
	VERSION 0.1.1
	LANGUAGES CXX
)

set(PROJECT_SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

# Options
option(WITH_SMAA_LIB_STATIC      "Enable SMAA static library"         ON)
option(WITH_SMAA_LIB_SHARED      "Enable SMAA shared library"         OFF)
option(WITH_SMAA_SAMPLE          "Enable command line sample program" ON)
option(WITH_SMAA_TESTS           "Enable SMAA filter tests"           ON)
option(WITH_SMAA_INSTALL_HEADERS "Install SMAA library headers"       OFF)
option(WITH_SMAA_INSTALL_BIN     "Install SMAA command line programs" OFF)

option(WITH_SMAA_SAMPLE_PREFER_SHLIB "Sample program prefers linking shared library" OFF)
mark_as_advanced(WITH_SMAA_SAMPLE_PREFER_SHLIB)

# Dependencies
if(WITH_SMAA_TESTS)
	set(WITH_SMAA_SAMPLE ON)
endif()

if(WITH_SMAA_SAMPLE)
	find_package(PNG REQUIRED)
	if(PNG_FOUND)
		if(NOT WITH_SMAA_LIB_SHARED AND NOT WITH_SMAA_LIB_STATIC)
			set(WITH_SMAA_LIB_STATIC ON)
		endif()
	else()
		message(WARNING "PNG library not found, disabling WITH_SMAA_SAMPLE")
		set(WITH_SMAA_SAMPLE OFF)
	endif()
endif()

if(WITH_SMAA_LIB_STATIC)
	set(WITH_SMAA_INSTALL_HEADERS ON)
endif()

# Subdirectories
add_subdirectory(bin)

if(WITH_SMAA_LIB_STATIC OR WITH_SMAA_LIB_SHARED)
	add_subdirectory(lib)
	add_subdirectory(include)
endif()

# Tests
if (WITH_SMAA_TESTS)
	enable_testing()
	add_subdirectory(tests)
	set(WITH_SMAA_SAMPLE ON)
endif()

# Packaging
#  Don't use ZIP generator if including shared library for Linux,
#  otherwise symlinks will be lost.
set(CPACK_PACKAGE_FILE_NAME ${PROJECT_NAME}_${PROJECT_VERSION}_${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR})
include(CPack)
