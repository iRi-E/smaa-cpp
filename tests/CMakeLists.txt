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

set(IMGDIR ${CMAKE_SOURCE_DIR}/tests)
set(TESTDIR ${CMAKE_BINARY_DIR}/tests)

file(GLOB FILES *_aa.png)
unset(IMAGES)
foreach(FILE IN LISTS FILES)
	get_filename_component(FILE ${FILE} NAME)
	string(REGEX REPLACE "_aa\\.png$" "" IMAGE ${FILE})
	list(APPEND IMAGES ${IMAGE})
endforeach()

foreach(IMAGE IN LISTS IMAGES)
	add_test(
		NAME filter_${IMAGE}
		COMMAND "$<TARGET_FILE:smaa_png>" ${IMGDIR}/${IMAGE}.png ${IMAGE}_result.png
		WORKING_DIRECTORY ${TESTDIR}
	)
endforeach()

foreach(IMAGE IN LISTS IMAGES)
	add_test(
		NAME compare_${IMAGE}
		COMMAND diff -s ${IMGDIR}/${IMAGE}_aa.png ${IMAGE}_result.png
		WORKING_DIRECTORY ${TESTDIR}
	)
endforeach()
