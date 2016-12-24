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

set(INCDIR ${CMAKE_SOURCE_DIR}/include)
set(GENDIR ${CMAKE_BINARY_DIR}/include)

include_directories(${INCDIR} ${GENDIR})

add_executable(smaa_areatex smaa_areatex.cpp)

if(WITH_SMAA_INSTALL_BIN)
	install(TARGETS smaa_areatex RUNTIME DESTINATION bin)
endif()

if(WITH_SMAA_SAMPLE)
	set(SRC
		smaa_png.cpp
		${INCDIR}/smaa.h
		${INCDIR}/smaa_types.h
		${GENDIR}/smaa_version.h
	)

	add_executable(smaa_png  ${SRC})
	include_directories(${PNG_INCLUDE_DIR})

	if ((WITH_SMAA_LIB_STATIC AND NOT WITH_SMAA_LIB_SHARED) OR
	    (WITH_SMAA_LIB_STATIC AND WITH_SMAA_LIB_SHARED AND NOT WITH_SMAA_SAMPLE_PREFER_SHLIB))
		target_link_libraries(smaa_png smaa-static ${PNG_LIBRARY})
	else()
		target_link_libraries(smaa_png smaa-shared ${PNG_LIBRARY})
	endif()

	if(WITH_SMAA_INSTALL_BIN)
		install(TARGETS smaa_png DESTINATION bin)
	endif()
endif()
