#
# This file was taken from RakNet 4.082.
# Please see licenses/RakNet license.txt for the underlying license and related copyright.
#
#
# Modified work: Copyright (c) 2016-2019, SLikeSoft UG (haftungsbeschränkt)
#
# This source code was modified by SLikeSoft. Modifications are licensed under the MIT-style
# license found in the license.txt file in the root directory of this source tree.
#

cmake_minimum_required(VERSION 3.8)

project(SLikeNetLibStatic)

FILE(GLOB ALL_HEADER_SRCS ${SLikeNet_SOURCE_DIR}/Source/include/slikenet/*.h)
FILE(GLOB ALL_COMPATIBILITY_HEADER_SRC ${SLikeNet_SOURCE_DIR}/Source/*.h)
FILE(GLOB ALL_COMPATIBILITY_HEADER_SRC_2 ${SLikeNet_SOURCE_DIR}/Source/slikenet/*.h)
FILE(GLOB ALL_CPP_SRCS ${SLikeNet_SOURCE_DIR}/Source/src/*.cpp)
FILE(GLOB CRYPTO_CPP_SRCS ${SLikeNet_SOURCE_DIR}/Source/src/crypto/*.cpp)
FILE(GLOB SQLITE_PLUGIN_CPP_SRCS ${SLikeNet_SOURCE_DIR}/Source/src/SQLite3Plugin/*.cpp)

add_library(SLikeNetLibStatic STATIC ${ALL_CPP_SRCS} ${CRYPTO_CPP_SRCS} ${SQLITE_PLUGIN_CPP_SRCS} ${ALL_HEADER_SRCS})

if(WIN32)
	target_include_directories(SLikeNetLibStatic PUBLIC ${SLIKENET_INTERNAL_INCLUDE_DIRS})
else()
	target_include_directories(SLikeNetLibStatic PUBLIC
	    $<BUILD_INTERFACE:${SLikeNet_SOURCE_DIR}/Source/include>
	    $<INSTALL_INTERFACE:include/SLikeNetLibStatic>
	)
endif()

# set the version number
set_target_properties(SLikeNetLibStatic PROPERTIES
	VERSION ${SLikeNet_VERSION}
	SOVERSION ${SLikeNet_API_VERSION}
)

# by default we build the retail configuration
add_definitions(-D_RETAIL)

IF(WIN32 AND NOT UNIX)
	add_definitions(-DWIN32 -D_RAKNET_DLL -D_CRT_NONSTDC_NO_DEPRECATE -D_CRT_SECURE_NO_DEPRECATE)
	SET( CMAKE_CXX_FLAGS "/GS- /GR-" )
ELSE(WIN32 AND NOT UNIX)
	set_target_properties(SLikeNetLibStatic PROPERTIES
		OUTPUT_NAME "slikenet"
	)
ENDIF(WIN32 AND NOT UNIX)

target_link_libraries(SLikeNetLibStatic ${SLIKENET_LIBRARY_LIBS})
IF(WIN32 AND NOT UNIX)
	IF(NOT ${CMAKE_GENERATOR} STREQUAL "MSYS Makefiles")
		set_target_properties(SLikeNetLibStatic PROPERTIES STATIC_LIBRARY_FLAGS "/NODEFAULTLIB:\"LIBCD.lib LIBCMTD.lib MSVCRT.lib\"" )
	ENDIF(NOT ${CMAKE_GENERATOR} STREQUAL "MSYS Makefiles")
ELSE(WIN32 AND NOT UNIX)
	configure_file(../../slikenet-config-version.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/slikenet-config-version.cmake @ONLY)

	INSTALL(TARGETS SLikeNetLibStatic EXPORT SLikeNetLibStatic DESTINATION lib/slikenet-${SLikeNet_VERSION})
	INSTALL(FILES ${ALL_COMPATIBILITY_HEADER_SRC} DESTINATION include/slikenet-${SLikeNet_VERSION})
	INSTALL(FILES ${ALL_COMPATIBILITY_HEADER_SRC_2} DESTINATION include/slikenet-${SLikeNet_VERSION}/slikenet)
	INSTALL(FILES ${ALL_HEADER_SRCS} DESTINATION include/slikenet-${SLikeNet_VERSION}/include/slikenet)
	INSTALL(FILES ../../slikenet-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/slikenet-config-version.cmake DESTINATION lib/slikenet-${SLikeNet_VERSION})
	INSTALL(EXPORT SLikeNetLibStatic FILE slikenet.cmake DESTINATION lib/slikenet-${SLikeNet_VERSION})
ENDIF(WIN32 AND NOT UNIX)
