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

set(INCDIR ../include)
set(GENDIR ${CMAKE_CURRENT_BINARY_DIR}/../include)

include_directories(${INCDIR} ${GENDIR})

set(GENSRC ${GENDIR}/smaa_areatex.h)
set(SRC
	smaa.cpp
	${INCDIR}/smaa.h
	${INCDIR}/smaa_types.h
	${GENSRC}
)

if(WITH_SUBPIXEL_RENDERING)
	set(AREATEX_OPTIONS -s)
	add_definitions(-DWITH_SUBPIXEL_RENDERING)
endif()

add_custom_command(
	OUTPUT ${GENSRC}
	COMMAND "$<TARGET_FILE:smaa_areatex>" ${AREATEX_OPTIONS} ${GENSRC}
	DEPENDS smaa_areatex
)
add_custom_target(smaa_areatex_header SOURCES ${GENSRC})

if(WITH_LIBRARY_STATIC)
	add_library(smaa-static STATIC ${SRC})
	set_target_properties(smaa-static
		PROPERTIES
		OUTPUT_NAME ${PROJECT_NAME}
		CLEAN_DIRECT_OUTPUT 1
		PREFIX "lib"
	)
	add_dependencies(smaa-static smaa_areatex_header)
	install(TARGETS smaa-static DESTINATION lib)
endif()

if(WITH_LIBRARY_SHARED)
	add_library(smaa-shared SHARED ${SRC})
	set_target_properties(smaa-shared
		PROPERTIES
		OUTPUT_NAME ${PROJECT_NAME}
		CLEAN_DIRECT_OUTPUT 1
		VERSION ${PROJECT_VERSION}
		SOVERSION ${PROJECT_SOVERSION}
	)
	add_dependencies(smaa-shared smaa_areatex_header)
	install(TARGETS smaa-shared DESTINATION lib)
endif()
