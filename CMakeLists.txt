# note: this is the oldest version tested against, even older versions might work
cmake_minimum_required(VERSION 3.13)

project(tibiarc
  VERSION 0.1
  LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS OFF)

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

add_compile_definitions("_REENTRANT" "_GNU_SOURCE")

# tibiarc
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
  "lib/deps/inflate.c"
  "lib/deps/inflate.h"
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


# converter
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
if(NOT NO_SDL2)
  find_package(SDL2 QUIET)
  if (NOT SDL2_FOUND)
    message(WARNING "SDL2 was not found, converter will be built without SDL2 encoder")
  endif()
endif()
if(SDL2_FOUND)
  target_include_directories(converter PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(converter PRIVATE ${SDL2_LIBRARIES})
else()
  target_compile_definitions(converter PRIVATE DISABLE_SDL2)
endif()

# ffmpeg
if(NOT NO_LIBAV)
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
  target_link_libraries(converter PRIVATE ${LIBAV_LIBRARIES})
else()
  target_compile_definitions(converter PRIVATE DISABLE_LIBAV)
endif()

# Enable IPO if supported
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)
if(ipo_supported)
  set_target_properties(tibiarc converter PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

