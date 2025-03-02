#
# Copyright 2025 "John Högberg"
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

function(check_tibia_data folder)
  if((EXISTS "${folder}/data/Tibia.dat") AND
     (EXISTS "${folder}/data/Tibia.pic") AND
     (EXISTS "${folder}/data/Tibia.spr"))
    set(TIBIARC_DATA_OK TRUE PARENT_SCOPE)
  else()
    set(TIBIARC_DATA_OK FALSE PARENT_SCOPE)
  endif()
endfunction()

function(add_converter_test folder version recording)
  ## Render one frame every two minutes; that will hopefully shake out the most
  ## egregious bugs without making tests take forever.
  ##
  ## This tests nearly everything in the library proper, but does not test that
  ## encoding works.
  add_test(NAME "converter: ${version}/${recording}"
           COMMAND converter
             --input-version ${version}
             --output-backend inert
             --frame-rate 1
             --frame-skip 120
             "${folder}/data"
             "${folder}/${recording}"
             "inert")
endfunction()

function(add_encode_test folder version recording)
  ## Renders a simple bitmap, this ought to be enough to cover the gist of the
  ## LibAV backend.
  ##
  ## This is just a smoke test: we do not check that the output looks good.
  if(LIBAV_FOUND)
    add_test(NAME "encode: ${version}/${recording}"
             COMMAND converter
               --input-version ${version}
               --resolution 640 352
               --output-format image2
               --output-encoding bmp
               --output-flags "update=1"
               --start-time 2000
               --end-time 2000
               --frame-rate 1
               "${folder}/data"
               "${folder}/${recording}"
               "${recording}.bmp"
               )
  endif()
endfunction()

function(add_graveyard_test data folder version recording)
  ## As add_converter_test, but it should fail since this is in the graveyard.
  add_test(NAME "graveyard-fail: ${version}/${recording}"
           COMMAND converter
             --input-version ${version}
             --output-backend inert
             --frame-rate 1
             --frame-skip 120
             "${data}"
             "${folder}/${recording}"
             "inert")
  set_property(TEST "graveyard-fail: ${version}/${recording}"
               PROPERTY WILL_FAIL true)

  ## The best-effort versioning of the graveyard is based on parsing the first
  ## two seconds, so all the versioned recordings in the graveyard should pass
  ## this test.
  add_test(NAME "graveyard-refine: ${version}/${recording}"
           COMMAND converter
             --input-version ${version}
             --input-partial
             --end-time 2000
             --output-backend inert
             --frame-rate 1
             --frame-skip 120
             "${data}"
             "${folder}/${recording}"
             "inert")
endfunction()

function(add_miner_test folder version recording)
  add_test(NAME "miner: ${version}/${recording}"
           COMMAND miner
             --input-version ${version} --dry-run
             "${folder}/data" "${folder}/${recording}")
endfunction()

function(scan_test_corpus)
  set(TIBIARC_RECORDING_ROOT "${PROJECT_SOURCE_DIR}/recordings")

  file(GLOB version_candidates LIST_DIRECTORIES TRUE
       RELATIVE "${TIBIARC_RECORDING_ROOT}/"
       "${TIBIARC_RECORDING_ROOT}/*.*/")
  foreach(version ${version_candidates})
    set(folder "${TIBIARC_RECORDING_ROOT}/${version}/")
    if(IS_DIRECTORY "${folder}")

      check_tibia_data(${folder})
      if(TIBIARC_DATA_OK)
        ## Find and test all the recordings of this version which we know can
        ## be fully processed without issue.
        file(GLOB recording_names LIST_DIRECTORIES TRUE
             RELATIVE "${folder}/" "${folder}/*.*")
        foreach(recording ${recording_names})
          if(NOT ("${recording}" EQUAL "data"))
            add_converter_test(${folder} ${version} ${recording})
            add_miner_test(${folder} ${version} ${recording})
          endif()
        endforeach()

        ## Find and test all recordings that we've determined belongs to this
        ## version, but cannot be processed in full.
        set(graveyard "${TIBIARC_RECORDING_ROOT}/graveyard/${version}/")
        file(GLOB recording_names LIST_DIRECTORIES TRUE
             RELATIVE "${graveyard}/" "${graveyard}/*.*")
        foreach(recording ${recording_names})
          if(NOT ("${recording}" EQUAL "data"))
            add_graveyard_test("${folder}/data"
                               ${graveyard}
                               ${version}
                               ${recording})
          endif()
        endforeach()
      else()
        ## Note that ${version_candidates} will be empty if the
        ## tibia-recordings submodule hasn't been fetched, so we will only
        ## emit this warning when someone clearly intends to run the full test
        ## suite.
        message(WARNING "Skipping tests for ${version}, no data files")
      endif()
    endif()
  endforeach()
endfunction()

block()
  ## In-tree recordings for quick smoke-testing.
  check_tibia_data("${PROJECT_SOURCE_DIR}/tests/8.40/")
  if(TIBIARC_DATA_OK)
    add_converter_test("${PROJECT_SOURCE_DIR}/tests/8.40/" "8.40" "sample.tmv2")
    add_encode_test("${PROJECT_SOURCE_DIR}/tests/8.40/" "8.40" "sample.tmv2")
    add_miner_test("${PROJECT_SOURCE_DIR}/tests/8.40/" "8.40" "sample.tmv2")
  else()
    message(WARNING "Skipping tests/8.40/sample.yatc, no Tibia data files in "
                    "tests/8.40/data/")
  endif()

  check_tibia_data("${PROJECT_SOURCE_DIR}/tests/8.50/")
  if(TIBIARC_DATA_OK)
    add_converter_test("${PROJECT_SOURCE_DIR}/tests/8.50/" "8.50" "sample.yatc")
    add_encode_test("${PROJECT_SOURCE_DIR}/tests/8.50/" "8.50" "sample.yatc")
    add_miner_test("${PROJECT_SOURCE_DIR}/tests/8.50/" "8.50" "sample.yatc")
  else()
    message(WARNING "Skipping tests/8.50/sample.yatc, no Tibia data files in "
                    "tests/8.50/data/")
  endif()
endblock()

## Scans the 'recordings/' submodule for in-depth testing of all supported
## versions.
scan_test_corpus()
