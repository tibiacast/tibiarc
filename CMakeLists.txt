#
# Copyright 2024 "Simon Sandström"
# Copyright 2024 "John Högberg"
#
# This file is part of tibiarc.
#
# tibiarc is free software: you can redistribute it and/or modify it under the
# terms of the GNU Affero General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.
#
# tibiarc is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
# details.
#
# You should have received a copy of the GNU Affero General Public License
# along with tibiarc. If not, see <https://www.gnu.org/licenses/>.
#

## Note that this is the oldest version tested against, even older versions
## might work.
cmake_minimum_required(VERSION 3.13)

project(tibiarc
  VERSION 0.1
  LANGUAGES C
)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(TIBIARC_CROSSCOMPILING "Whether this is a cross-compiled build" OFF)
if(TIBIARC_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING TRUE)
  set(TIBIARC_NO_LTO TRUE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)

# ########################################################################### #
## `tibiarc` library
# ########################################################################### #

# Read L1 cache line size
execute_process(
  COMMAND getconf LEVEL1_DCACHE_LINESIZE
  OUTPUT_VARIABLE LEVEL1_DCACHE_LINESIZE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE RET
)
if(RET AND NOT RET EQUAL 0)
  set(LEVEL1_DCACHE_LINESIZE "128")
endif()

add_library(tibiarc
  "lib/canvas.c"
  "lib/canvas.h"
  "lib/characterset.h"
  "lib/container.c"
  "lib/container.h"
  "lib/creature.c"
  "lib/creature.h"
  "lib/datareader.c"
  "lib/datareader.h"
  "lib/deps/uthash.h"
  "lib/effect.h"
  "lib/fonts.c"
  "lib/fonts.h"
  "lib/formats/tibiacast.c"
  "lib/formats/tibiareplay.c"
  "lib/formats/tmv2.c"
  "lib/formats/yatc.c"
  "lib/gamestate.c"
  "lib/gamestate.h"
  "lib/icons.c"
  "lib/icons.h"
  "lib/map.h"
  "lib/message.c"
  "lib/message.h"
  "lib/missile.h"
  "lib/object.h"
  "lib/parser.c"
  "lib/parser.h"
  "lib/pictures.c"
  "lib/pictures.h"
  "lib/pixel.h"
  "lib/player.h"
  "lib/position.h"
  "lib/recordings.c"
  "lib/recordings.h"
  "lib/renderer.c"
  "lib/renderer.h"
  "lib/report.c"
  "lib/report.h"
  "lib/sprites.c"
  "lib/sprites.h"
  "lib/textrenderer.c"
  "lib/textrenderer.h"
  "lib/tile.c"
  "lib/tile.h"
  "lib/types.c"
  "lib/types.h"
  "lib/utils.h"
  "lib/versions.c"
  "lib/versions_decl.h"
  "lib/versions.h"
)
target_include_directories(tibiarc PUBLIC "lib")
target_link_libraries(tibiarc PRIVATE m)
target_compile_definitions(tibiarc PUBLIC "LEVEL1_DCACHE_LINESIZE=${LEVEL1_DCACHE_LINESIZE}")

# Options to help debug certain versioning issues, enable them with e.g.
# `cmake -DTIBIARC_DUMP_PIC=ON ...`
option(TIBIARC_DUMP_PIC "Dumps .pic files as bitmaps when loaded" OFF)
if(TIBIARC_DUMP_PIC)
  target_compile_definitions(tibiarc PRIVATE TIBIARC_DUMP_PIC)
endif()
option(TIBIARC_DUMP_MESSAGE_TYPES "Writes messages and their types to stdout when received" OFF)
if(TIBIARC_DUMP_MESSAGE_TYPES)
  target_compile_definitions(tibiarc PRIVATE TIBIARC_DUMP_MESSAGE_TYPES)
endif()
option(TIBIARC_DUMP_ITEMS "Dumps item sprites (with properties) as bitmaps when loaded" OFF)
if(TIBIARC_DUMP_ITEMS)
  target_compile_definitions(tibiarc PRIVATE TIBIARC_DUMP_ITEMS)
endif()

# OpenMP support
option(TIBIARC_NO_OPENMP "Explicitly disable OpenMP support" OFF)
if(NOT TIBIARC_NO_OPENMP)
  find_package(OpenMP QUIET)
endif()
if(OpenMP_C_FOUND)
  target_include_directories(tibiarc PRIVATE ${OpenMP_C_INCLUDE_DIRS})
  target_compile_options(tibiarc PRIVATE ${OpenMP_C_FLAGS})
  target_link_libraries(tibiarc PRIVATE ${OpenMP_C_LIBRARIES})
endif()

option(TIBIARC_NO_ZLIB "Explicitly disable zlib support" OFF)
if(NOT TIBIARC_NO_ZLIB)
  find_package(ZLIB QUIET)
  if (NOT ZLIB_FOUND)
    message(WARNING "zlib was not found, tibiarc will be built without zlib support")
  endif()
endif()
if(ZLIB_FOUND)
  target_include_directories(tibiarc PRIVATE ${ZLIB_INCLUDE_DIRS})
  target_link_libraries(tibiarc PRIVATE ${ZLIB_LIBRARIES})
else()
  target_compile_definitions(tibiarc PRIVATE DISABLE_ZLIB)
endif()

# ########################################################################### #
## Recording converter
# ########################################################################### #

add_executable(converter
  "converter/converter.c"
  "converter/encoders/inert.c"
  "converter/encoders/libav.c"
  "converter/encoders/sdl2.c"
  "converter/encoding.c"
  "converter/encoding.h"
  "converter/exporter.c"
  "converter/exporter.h"
  "converter/memoryfile.c"
  "converter/memoryfile.h"
)

target_include_directories(converter PRIVATE "converter")
target_link_libraries(converter PRIVATE tibiarc)

# SDL2
option(TIBIARC_NO_SDL2 "Explicitly disable SDL2" OFF)
if(NOT TIBIARC_NO_SDL2)
  find_package(SDL2 QUIET)
  if (NOT SDL2_FOUND)
    message(WARNING "SDL2 was not found, converter will be built without SDL2 encoder")
  endif()
endif()
if(SDL2_FOUND)
  target_include_directories(converter PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(converter PRIVATE SDL2::SDL2)
else()
  target_compile_definitions(converter PRIVATE DISABLE_SDL2)
endif()

# ffmpeg
option(TIBIARC_NO_LIBAV "Explicitly disable ffmpeg" OFF)
if(NOT TIBIARC_NO_LIBAV)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBAV IMPORTED_TARGET
    libavcodec
    libavformat
    libswscale
    libavutil
  )
  if(NOT LIBAV_FOUND)
    message(WARNING "ffmpeg libraries were not found, converter will be built without ffmpeg encoder")
  endif()
endif()
if(LIBAV_FOUND)
  target_include_directories(converter PRIVATE ${LIBAV_INCLUDE_DIRS})
  target_link_libraries(converter PRIVATE PkgConfig::LIBAV)
else()
  target_compile_definitions(converter PRIVATE DISABLE_LIBAV)
endif()

# ########################################################################### #
## player
# ########################################################################### #

add_executable(player
  "player/player.c"
  "player/playback.c"
  "player/playback.h"
  "player/rendering.c"
  "player/rendering.h"
  "converter/memoryfile.c"
  "converter/memoryfile.h"
)
target_include_directories(player PRIVATE "converter")
target_link_libraries(player PRIVATE tibiarc)

if (NOT EMSCRIPTEN)
  if (NOT SDL2_FOUND)
    message(FATAL_ERROR "player without EMSCRIPTEN requires SDL2")
  endif()
  target_include_directories(player PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(player PRIVATE ${SDL2_LIBRARIES})
else()
  set_target_properties(converter PROPERTIES EXCLUDE_FROM_ALL 1)
  set_target_properties(player PROPERTIES SUFFIX ".html")
  target_compile_options(player PRIVATE -sUSE_SDL=2)
  target_link_options(player PRIVATE -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_FUNCTIONS=_main,_load_files,_malloc -sEXPORTED_RUNTIME_METHODS=ccall)
  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_options(player PRIVATE -sEXCEPTION_DEBUG=1 -sASSERTIONS=1 -sSAFE_HEAP=1 -sDEMANGLE_SUPPORT=1)
  endif()
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/player/index.html"  "${CMAKE_BINARY_DIR}/index.html" COPYONLY)
endif()


# ########################################################################### #
## Misc flags that must be set at the very end
# ########################################################################### #

# Enable IPO if supported
option(TIBIARC_NO_LTO "Explicitly disable link-time optimization" OFF)
if(NOT TIBIARC_NO_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported)
  if(ipo_supported)
    set_target_properties(tibiarc converter PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    if (NOT EMSCRIPTEN)
      set_target_properties(player PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
  endif()
endif()

# Include extra CMakeLists.txt if available
include(CMakeLists.user.txt OPTIONAL)
