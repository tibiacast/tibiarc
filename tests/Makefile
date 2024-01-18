#
# Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
# Copyright 2023-2024 "John Högberg"
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

##
## FIXME: Once we've moved to CTest, see if we can make use of `ExternalData`
## to fetch data files, a larger amount of recordings, and so on.
##

test: test_encode test_sdl2 test_screenshot test_versions
update: update_screenshot

.PHONY: clean
clean:
	git clean -fdxq render/ logs/

# Simplifies running tests under tools like perf, rr, valgrind, et al.
# `make TEST_PREFIX=valgrind all`
TRC_TEST := $(TEST_PREFIX) ../tibiarc

# ########################################################################### #

ENCODE_FILES := render/encode_0.mkv render/encode_1.mkv render/encode_2.mkv
test_encode: $(ENCODE_FILES)

# Basic, with format guessing
render/encode_0.mkv:
	$(TRC_TEST) --output-format matroska \
              --output-encoding libx265 \
              ./8.40/ \
              ./8.40/sample.tmv2 \
              $@

# High-res high-framerate
render/encode_1.mkv:
	$(TRC_TEST) --input-format tmv2 \
              --resolution 1600x1056 \
              --frame-rate 50 \
              --output-format matroska \
              --output-encoding libx265 \
              ./8.40/ \
              ./8.40/sample.tmv2 \
              $@

# Complex encode flags (see ffmpeg docs for a full listing)
render/encode_2.mkv:
	$(TRC_TEST) --input-format tmv2 \
              --output-format matroska \
              --output-encoding libx264 \
              --output-flags "b=20000000:flags=cgop+global_header:profile=Baseline" \
              ./8.40/ \
              ./8.40/sample.tmv2 \
              $@

# ########################################################################### #

SDL2_TESTS := test_sdl2_0 test_sdl2_1
.PHONY: $(SDL2_TESTS)
test_sdl2: $(SDL2_TESTS)

test_sdl2_0:
	$(TRC_TEST) --input-format tmv2 \
              --output-backend sdl2 \
              --resolution 960x704 \
              --frame-rate 25 \
              ./8.40/ \
              ./8.40/sample.tmv2

test_sdl2_1:
	$(TRC_TEST) --input-format yatc \
              --input-version 8.50 \
              --output-backend sdl2 \
              --resolution 1120x704 \
              --frame-skip 16 \
              ./8.50/ \
              ./8.50/sample.yatc

# ########################################################################### #

SCREENSHOT_FILES := render/screenshot_0.bmp render/screenshot_1.bmp
SCREENSHOT_CHECKSUMS := $(addsuffix .sha256sum, $(SCREENSHOT_FILES))
.PHONY: test_screenshot
test_screenshot: $(SCREENSHOT_FILES)
	sha256sum -c $(SCREENSHOT_CHECKSUMS)
update_screenshot: $(SCREENSHOT_CHECKSUMS)

# Screenshot as it would be rendered in a normal video.
render/screenshot_0.bmp:
	$(TRC_TEST) --input-format tmv2 \
              --resolution 640x352 \
              --output-format image2 \
              --output-encoding bmp \
              --output-flags "update=1" \
              --start-time 2000 \
              --end-time 2000 \
              --frame-rate 1 \
              ./8.40/ \
              ./8.40/sample.tmv2 \
              $@

test_screenshot_0: render/screenshot_0.bmp
	sha256sum -c render/screenshot_0.bmp.sha256sum

render/screenshot_%.bmp.sha256sum: render/screenshot_%.bmp
	sha256sum $< > $@

# Skip rendering cluttery effects that look bad when scaled down, generating an
# image suitable for use as a thumbnail.
render/screenshot_1.bmp:
	$(TRC_TEST) --input-format tmv2 \
              --output-encoding bmp \
              --output-format image2 \
              --output-flags "update=1" \
              --start-time 3000 \
              --end-time 3000 \
              --frame-rate 1 \
              --skip-rendering-inventory \
              --skip-rendering-icon-bar \
              --skip-rendering-status-bars \
              --skip-rendering-messages \
              --skip-rendering-player-names \
              --skip-rendering-creature-health-bars \
              --skip-rendering-creature-names \
              --skip-rendering-creature-icons \
              ./8.40/ \
              ./8.40/sample.tmv2 \
              $@

test_screenshot_1: render/screenshot_1.bmp
	sha256sum -c render/screenshot_1.bmp.sha256sum

update_test_screenshot_1: render/screenshot_1.bmp
	sha256sum render/screenshot_1.bmp > render/screenshot_1.bmp.sha256sum

# ########################################################################### #

# Tests that all saved recordings parse in their entirety, skipping rendering
# because it would be too slow. Run these with DUMP_MSG_TYPES=1 to dump text
# messages for each version, helping us map them out.
VERSION_TEST_FLAGS := --output-backend inert

VERSION_TEST_DATA_FILES := $(wildcard ./*/Tibia.dat) \
                           $(wildcard ./*/Tibia.pic) \
                           $(wildcard ./*/Tibia.spr)
VERSION_TEST_RECORDINGS_ := $(wildcard ./*.*/*)

VERSION_TEST_RECORDINGS := $(filter-out \
                             $(VERSION_TEST_DATA_FILES), \
                             $(VERSION_TEST_RECORDINGS_))

VERSION_TEST_LOGS := $(addsuffix .log, \
                       $(addprefix logs/,\
                         $(notdir $(VERSION_TEST_RECORDINGS))))

# $1 = recording, $2 = version, $3 = target log file
define VERSION_TEST
$(3):
	$(TRC_TEST) $(VERSION_TEST_FLAGS) \
            --input-version $(2) \
            $(dir $(1)) $(1) 2>&1 | tee $(3) > /dev/null
endef

$(foreach _recording, $(VERSION_TEST_RECORDINGS), \
  $(eval $(call VERSION_TEST, \
           $(_recording), \
           $(shell basename $(dir $(_recording))), \
           $(addsuffix .log, $(addprefix logs/, $(notdir $(_recording)))))))

test_versions: $(VERSION_TEST_LOGS)
