#
# Copyright 2024 "Simon Sandström"
# Copyright 2024-2025 "John Högberg"
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
cmake_minimum_required(VERSION 3.25)

project(tibiarc
  VERSION 0.1
  LANGUAGES C CXX
)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(TIBIARC_CROSSCOMPILING "Whether this is a cross-compiled build" OFF)
if(TIBIARC_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING TRUE)
  set(TIBIARC_NO_LTO TRUE)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 11)

# ########################################################################### #
## Optional dependency handling
# ########################################################################### #

option(TIBIARC_NO_LIBAV "Explicitly disable ffmpeg" OFF)
if(NOT TIBIARC_NO_LIBAV)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBAV IMPORTED_TARGET
      libavcodec
      libavformat
      libswscale
      libavutil
    )
  endif()
  if(NOT LIBAV_FOUND)
    message(WARNING "ffmpeg libraries were not found, converter will be built without ffmpeg encoder")
  endif()
endif()

option(TIBIARC_NO_OPENMP "Explicitly disable OpenMP support" OFF)
if(NOT TIBIARC_NO_OPENMP)
  find_package(OpenMP QUIET)
endif()

option(TIBIARC_NO_TBB "Explicitly disable TBB support" OFF)
if(NOT TIBIARC_NO_TBB)
  find_package(TBB QUIET)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  option(TIBIARC_NO_BCRYPT "Explicitly disable bcrypt support" OFF)

  if(NOT TIBIARC_NO_BCRYPT)
    include(CheckCXXSourceCompiles)

    check_cxx_source_compiles("extern \"C\" {
                                 #include <windows.h>
                                 #include <ntstatus.h>
                                 #include <bcrypt.h>
                               }
                               int main() {return 0;}"
                              BCRYPT_FOUND)
    if(NOT BCRYPT_FOUND)
      message(WARNING "bcrypt was not found")
    endif()
  endif()
endif()

option(TIBIARC_NO_OPENSSL "Explicitly disable libcrypto support" OFF)
if(NOT TIBIARC_NO_OPENSSL)
  find_package(OpenSSL QUIET)
endif()

option(TIBIARC_NO_SDL2 "Explicitly disable SDL2" OFF)
if(NOT TIBIARC_NO_SDL2)
  find_package(SDL2 QUIET)
  if(NOT (SDL2_FOUND OR EMSCRIPTEN))
    message(WARNING "Neither SDL2 nor emscripten was found, player will not be built")
  endif()
endif()

option(TIBIARC_NO_ZLIB "Explicitly disable zlib support" OFF)
if(NOT TIBIARC_NO_ZLIB)
  find_package(ZLIB QUIET)
  if(NOT ZLIB_FOUND)
    message(WARNING "zlib was not found, tibiarc will be built without zlib support")
  endif()
endif()

option(TIBIARC_NO_QT "Explicitly disable Qt support" OFF)
if(NOT TIBIARC_NO_QT)
  find_package(Qt6 COMPONENTS Concurrent Widgets QUIET)
  if(Qt6Widgets_VERSION VERSION_LESS 6.9.0)
    set(Qt6Widgets_FOUND FALSE)
  endif()
endif()

set(TIBIARC_HAVE_QT_CONCURRENT (Qt6Concurrent_FOUND AND (NOT EMSCRIPTEN)))

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
  "lib/canvas.cpp"
  "lib/canvas.hpp"
  "lib/characterset.cpp"
  "lib/characterset.hpp"
  "lib/container.hpp"
  "lib/creature.hpp"
  "lib/crypto.cpp"
  "lib/crypto.hpp"
  "lib/datareader.hpp"
  "lib/demuxer.hpp"
  "lib/deps/7z/7zTypes.h"
  "lib/deps/7z/Alloc.c"
  "lib/deps/7z/Alloc.h"
  "lib/deps/7z/Compiler.h"
  "lib/deps/7z/LzmaDec.c"
  "lib/deps/7z/LzmaDec.h"
  "lib/deps/7z/Lzma2Dec.c"
  "lib/deps/7z/Lzma2Dec.h"
  "lib/deps/7z/Precomp.h"
  "lib/effect.hpp"
  "lib/events.cpp"
  "lib/events.hpp"
  "lib/fonts.cpp"
  "lib/fonts.hpp"
  "lib/formats/cam.cpp"
  "lib/formats/rec.cpp"
  "lib/formats/tibiacast.cpp"
  "lib/formats/tibiareplay.cpp"
  "lib/formats/tmv2.cpp"
  "lib/formats/tmv1.cpp"
  "lib/formats/ttm.cpp"
  "lib/formats/yatc.cpp"
  "lib/gamestate.cpp"
  "lib/gamestate.hpp"
  "lib/icons.cpp"
  "lib/icons.hpp"
  "lib/map.hpp"
  "lib/message.cpp"
  "lib/message.hpp"
  "lib/missile.hpp"
  "lib/object.hpp"
  "lib/parser.cpp"
  "lib/parser.hpp"
  "lib/pictures.cpp"
  "lib/pictures.hpp"
  "lib/pixel.hpp"
  "lib/player.hpp"
  "lib/position.hpp"
  "lib/recordings.cpp"
  "lib/recordings.hpp"
  "lib/renderer.cpp"
  "lib/renderer.hpp"
  "lib/sprites.cpp"
  "lib/sprites.hpp"
  "lib/textrenderer.cpp"
  "lib/textrenderer.hpp"
  "lib/tile.cpp"
  "lib/tile.hpp"
  "lib/types.cpp"
  "lib/types.hpp"
  "lib/utils.hpp"
  "lib/versions_decl.hpp"
  "lib/versions.cpp"
  "lib/versions.hpp"
)
target_include_directories(tibiarc PUBLIC "lib")
target_compile_definitions(tibiarc PUBLIC "LEVEL1_DCACHE_LINESIZE=${LEVEL1_DCACHE_LINESIZE}")

if(MSVC)
  target_compile_options(tibiarc PRIVATE /W1)
else()
  target_link_libraries(tibiarc PRIVATE m)
  target_compile_options(tibiarc PRIVATE -Wall -Wextra)
endif()

option(TIBIARC_SANITIZE "Whether to enable address & UB sanitizers" OFF)
if(TIBIARC_SANITIZE)
  target_compile_options(tibiarc PUBLIC -fsanitize=address,undefined -fsanitize-trap=all)
  target_link_options(tibiarc PUBLIC -fsanitize=address,undefined)
endif()

option(TIBIARC_COVERAGE "Whether to enable code coverage" OFF)
if(TIBIARC_COVERAGE)
  target_compile_options(tibiarc PUBLIC -coverage)
  target_link_options(tibiarc PUBLIC -coverage)
endif()

# Options to help debug certain versioning issues, enable them with e.g.
# `cmake -DTIBIARC_DUMP_PIC=ON ...`
option(TIBIARC_DUMP_PIC "Dumps .pic files as bitmaps when loaded" OFF)
if(TIBIARC_DUMP_PIC)
  target_compile_definitions(tibiarc PRIVATE DUMP_PIC)
endif()

option(TIBIARC_DUMP_ITEMS "Dumps item sprites (with properties) as bitmaps when loaded" OFF)
if(TIBIARC_DUMP_ITEMS)
  target_compile_definitions(tibiarc PRIVATE DUMP_ITEMS)
endif()

set(TIBIARC_HAVE_CRYPTO FALSE)

if(BCRYPT_FOUND)
  target_link_libraries(tibiarc PRIVATE bcrypt)

  target_compile_definitions(tibiarc PRIVATE DISABLE_OPENSSL_CRYPTO)
  target_compile_definitions(tibiarc PRIVATE DISABLE_QT_CRYPTO)
  set(TIBIARC_HAVE_CRYPTO TRUE)
else()
  if(OPENSSL_FOUND)
    target_include_directories(tibiarc PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(tibiarc PRIVATE OpenSSL::Crypto)

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      target_link_libraries(tibiarc PRIVATE crypt32 ws2_32)
    endif()

    target_compile_definitions(tibiarc PRIVATE DISABLE_BCRYPT_CRYPTO)
    target_compile_definitions(tibiarc PRIVATE DISABLE_QT_CRYPTO)
    set(TIBIARC_HAVE_CRYPTO TRUE)
  else()
    if(Qt6Core_FOUND)
      target_include_directories(tibiarc PRIVATE ${QT6_INCLUDE_DIRS})
      target_link_libraries(tibiarc PUBLIC Qt6::Core)

      target_compile_definitions(tibiarc PRIVATE DISABLE_BCRYPT_CRYPTO)
      target_compile_definitions(tibiarc PRIVATE DISABLE_OPENSSL_CRYPTO)
      set(TIBIARC_HAVE_CRYPTO TRUE)
    endif()
  endif()
endif()

if(NOT TIBIARC_HAVE_CRYPTO)
  message(WARNING "No supported crypto library found, tibiarc will be built without .rec support")
  target_compile_definitions(tibiarc PRIVATE DISABLE_BCRYPT_CRYPTO)
  target_compile_definitions(tibiarc PRIVATE DISABLE_OPENSSL_CRYPTO)
  target_compile_definitions(tibiarc PRIVATE DISABLE_QT_CRYPTO)
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

if(NOT EMSCRIPTEN)
  add_executable(converter
    "converter/converter.cpp"
    "converter/encoders/inert.cpp"
    "converter/encoders/inert.hpp"
    "converter/encoders/libav.cpp"
    "converter/encoders/libav.hpp"
    "converter/encoding.cpp"
    "converter/encoding.hpp"
    "converter/exporter.cpp"
    "converter/exporter.hpp"
    "utils/cli.hpp"
    "utils/memoryfile.cpp"
    "utils/memoryfile.hpp"
  )

  target_include_directories(converter PRIVATE "converter" "utils")
  target_link_libraries(converter PRIVATE tibiarc)

  if(LIBAV_FOUND)
    target_include_directories(converter PRIVATE ${LIBAV_INCLUDE_DIRS})
    target_link_libraries(converter PRIVATE PkgConfig::LIBAV)
  else()
    target_compile_definitions(converter PRIVATE DISABLE_LIBAV)
  endif()

  if(OpenMP_FOUND)
    target_include_directories(converter PRIVATE ${OpenMP_CXX_INCLUDE_DIRS})
    target_compile_options(converter PRIVATE ${OpenMP_CXX_FLAGS})
    target_link_libraries(converter PRIVATE ${OpenMP_CXX_LIBRARIES})
  endif()
endif()

# ########################################################################### #
## Miner
# ########################################################################### #

if(NOT EMSCRIPTEN)
  add_executable(miner
    "deps/json.hpp"
    "miner/miner.cpp"
    "miner/serializer.cpp"
    "miner/serializer.hpp"
    "utils/cli.hpp"
    "utils/memoryfile.cpp"
    "utils/memoryfile.hpp"
  )

  target_include_directories(miner PRIVATE "miner" "deps" "utils")
  target_link_libraries(miner PRIVATE tibiarc)
endif()

# ########################################################################### #
## Collator
# ########################################################################### #

if(TIBIARC_HAVE_CRYPTO AND (NOT EMSCRIPTEN))
  add_executable(collator
    "collator/collator.cpp"
    "utils/cli.hpp"
    "utils/collation.cpp"
    "utils/collation.hpp"
    "utils/memoryfile.cpp"
    "utils/memoryfile.hpp"
  )
  target_include_directories(collator PRIVATE "collator" "utils")
  target_link_libraries(collator PRIVATE tibiarc)

  if(OPENSSL_FOUND)
    target_include_directories(collator PRIVATE ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(collator PRIVATE OpenSSL::Crypto)
  endif()

  # Required for `std::execution::par` to work properly with gcc/clang
  if(TBB_FOUND)
    target_link_libraries(collator PRIVATE TBB::tbb)
  endif()
endif()

if(Qt6Widgets_FOUND AND (EMSCRIPTEN OR Qt6Concurrent_FOUND))
  qt_standard_project_setup()

  qt_add_executable(gui
    "deps/json.hpp"
    "gui/collection.cpp"
    "gui/collection.hpp"
    "gui/database.cpp"
    "gui/database.hpp"
    "gui/main.cpp"
    "gui/mediacontrols.cpp"
    "gui/mediacontrols.hpp"
    "gui/player.cpp"
    "gui/player.hpp"
    "gui/window.cpp"
    "gui/window.hpp"
    "utils/collation.cpp"
    "utils/collation.hpp"
    "utils/memoryfile.cpp"
    "utils/memoryfile.hpp"
  )

  target_include_directories(gui PRIVATE "gui"
                                         "deps"
                                         "utils"
                                         ${QT6_INCLUDE_DIRS})
  target_link_libraries(gui PRIVATE tibiarc Qt6::Widgets)

  if(TIBIARC_HAVE_QT_CONCURRENT)
    target_link_libraries(gui PRIVATE Qt6::Concurrent)
  else()
    target_compile_definitions(gui PRIVATE DISABLE_QT_CONCURRENT)
  endif()

  if(EMSCRIPTEN)
    target_link_options(gui PUBLIC -sASYNCIFY
                                   -sASSERTIONS
                                   -sNO_DISABLE_EXCEPTION_CATCHING
                                   -Os)
    target_link_libraries(gui PRIVATE idbfs.js)
  endif()
endif()

# ########################################################################### #
## Misc flags that must be set at the very end
# ########################################################################### #

# Include extra CMakeLists.txt if available
include(CMakeLists.user.txt OPTIONAL)

# ########################################################################### #
## Tests
# ########################################################################### #

if(BUILD_TESTING)
  include(CTest)
  include(tests/CTestConfig.cmake)
endif()
