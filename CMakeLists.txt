# Extreme Tux Racer - WebAssembly / WebGL 2 build.
#
#   emcmake cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j
#
# The output in build/web/ (index.html, etr.js, etr.wasm, etr.data) can be
# served by any static web server.

cmake_minimum_required(VERSION 3.16)
project(ExtremeTuxRacerWeb CXX C)

if(NOT EMSCRIPTEN)
	message(FATAL_ERROR "This project targets the web. Configure it with emcmake (Emscripten).")
endif()

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ETR_DEBUG "Build with Emscripten assertions and debug info" OFF)

file(GLOB ETR_GAME_SOURCES ${CMAKE_SOURCE_DIR}/src/*.cpp)
set(ETR_PORT_SOURCES
	${CMAKE_SOURCE_DIR}/port/glcompat/gl_compat.cpp
	${CMAKE_SOURCE_DIR}/port/sfml/src/System.cpp
	${CMAKE_SOURCE_DIR}/port/sfml/src/Window.cpp
	${CMAKE_SOURCE_DIR}/port/sfml/src/Graphics.cpp
	${CMAKE_SOURCE_DIR}/port/sfml/src/Audio.cpp
	${CMAKE_SOURCE_DIR}/port/web/web_platform.cpp
)

add_executable(etr ${ETR_GAME_SOURCES} ${ETR_PORT_SOURCES})

target_include_directories(etr PRIVATE
	${CMAKE_SOURCE_DIR}/port/include        # GL/gl.h, GL/glu.h -> compat layer
	${CMAKE_SOURCE_DIR}/port/sfml/include   # SFML/*.hpp
	${CMAKE_SOURCE_DIR}/third_party/stb
)

# ETR uses C++ exceptions; native WebAssembly exception handling is
# supported by all current browsers.
target_compile_options(etr PRIVATE -fwasm-exceptions
	-Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-overloaded-virtual)
# stb headers trigger a few harmless warnings
set_source_files_properties(
	${CMAKE_SOURCE_DIR}/port/sfml/src/Graphics.cpp
	${CMAKE_SOURCE_DIR}/port/sfml/src/Window.cpp
	${CMAKE_SOURCE_DIR}/port/sfml/src/Audio.cpp
	PROPERTIES COMPILE_OPTIONS "-Wno-unused-parameter;-Wno-sign-compare;-Wno-missing-field-initializers")

set(ETR_WEB_DIR ${CMAKE_BINARY_DIR}/web)
set_target_properties(etr PROPERTIES
	OUTPUT_NAME etr
	SUFFIX ".html"
	RUNTIME_OUTPUT_DIRECTORY ${ETR_WEB_DIR})

set(ETR_LINK_FLAGS
	-fwasm-exceptions
	-sMIN_WEBGL_VERSION=2
	-sMAX_WEBGL_VERSION=2
	-sALLOW_MEMORY_GROWTH=1
	-sINITIAL_MEMORY=128MB
	-sSTACK_SIZE=4MB
	-sENVIRONMENT=web
	-sEXPORTED_FUNCTIONS=_main,_etr_save_progress
	-sEXPORTED_RUNTIME_METHODS=FS
	-lidbfs.js
	--pre-js ${CMAKE_SOURCE_DIR}/port/web/pre.js
	--shell-file ${CMAKE_SOURCE_DIR}/port/web/shell.html
	"SHELL:--preload-file ${CMAKE_SOURCE_DIR}/data@/data"
	"SHELL:--exclude-file *Makefile*"
)

if(ETR_DEBUG)
	target_compile_options(etr PRIVATE -g -O1)
	list(APPEND ETR_LINK_FLAGS -g -O1 -sASSERTIONS=2 -sGL_ASSERTIONS=1)
else()
	target_compile_options(etr PRIVATE -O3)
	list(APPEND ETR_LINK_FLAGS -O3)
endif()

target_link_options(etr PRIVATE ${ETR_LINK_FLAGS})

# Rebuild when the shell or runtime glue change.
set_property(TARGET etr APPEND PROPERTY LINK_DEPENDS
	${CMAKE_SOURCE_DIR}/port/web/shell.html
	${CMAKE_SOURCE_DIR}/port/web/pre.js)

# Serve as index.html.
add_custom_command(TARGET etr POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy ${ETR_WEB_DIR}/etr.html ${ETR_WEB_DIR}/index.html
	COMMENT "Creating index.html")
