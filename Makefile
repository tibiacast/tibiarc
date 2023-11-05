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

# We use MXE to cross-compile to Windows, see https://mxe.cc/
#
# $ export PATH=/path/to/MXE/usr/bin:$PATH
# $ make CC=gcc CROSS=x86_64-w64-mingw32.static- # Note dash at the end!
TRC_CC=$(CROSS)$(CC)
TRC_AR=$(CROSS)$(AR)
PKG_CONFIG=$(CROSS)pkg-config

LIBAV_NAMES = libavcodec libavformat libswscale libavutil
SDL2_NAMES = sdl2

ifeq ($(shell $(PKG_CONFIG) --exists $(LIBAV_NAMES); echo $$?), 1)
NO_LIBAV := 1
endif

ifeq ($(shell $(PKG_CONFIG) --exists $(SDL2_NAMES);echo $$?), 1)
NO_SDL2 := 1
endif

ifneq ($(NO_LIBAV), 1)
CONVERTER_CFLAGS += $(shell $(PKG_CONFIG) --silence-errors --cflags $(LIBAV_NAMES))
CONVERTER_LIBS += $(shell $(PKG_CONFIG) --silence-errors --libs $(LIBAV_NAMES))
else
CONVERTER_CFLAGS += -DDISABLE_LIBAV
endif

ifneq ($(NO_SDL2), 1)
CONVERTER_CFLAGS += $(shell $(PKG_CONFIG) --silence-errors --cflags $(SDL2_NAMES))
CONVERTER_LIBS += $(shell $(PKG_CONFIG) --silence-errors --libs $(SDL2_NAMES))
else
CONVERTER_CFLAGS += -DDISABLE_SDL2
endif

ifdef SANITIZE
CFLAGS += -fsanitize=undefined -fsanitize=address
endif

ifdef DEBUG
CFLAGS += -DDEBUG -O0
else ifndef CROSS
CFLAGS += -flto -march=native 
else
CFLAGS += -O2
endif

ifdef NO_RENDER
CFLAGS += -DDISABLE_RENDERING
endif

ifdef NO_ERROR_REPORTING
CFLAGS += -DDISABLE_ERROR_REPORTING
endif

ifdef DUMP_PIC
CFLAGS += -DDUMP_PIC
endif

ifdef DUMP_MSG_TYPES
CFLAGS += -DDUMP_MESSAGE_TYPES
endif

ifdef DUMP_ITEMS
CFLAGS += -DDUMP_ITEMS
endif

CFLAGS += --std=c11 -D_GNU_SOURCE -g -Wall
CFLAGS += -DLEVEL1_DCACHE_LINESIZE=$(shell getconf LEVEL1_DCACHE_LINESIZE || echo 64)

OBJDIR := obj

LIB_DEPS_HDR_FILES := $(wildcard lib/deps/*.h)
LIB_DEPS_SRC_FILES := $(wildcard lib/deps/*.c)
LIB_DEPS_OBJ_FILES := $(addprefix $(OBJDIR)/, \
                        $(notdir $(LIB_DEPS_SRC_FILES:.c=.o)))

LIB_HDR_FILES_1 := $(wildcard lib/*/*.h) $(wildcard lib/*.h)
LIB_HDR_FILES := $(filter-out $(LIB_DEPS_HDR_FILES), $(LIB_HDR_FILES_1))
LIB_SRC_FILES_1 := $(wildcard lib/*/*.c) $(wildcard lib/*.c)
LIB_SRC_FILES := $(filter-out $(LIB_DEPS_SRC_FILES), $(LIB_SRC_FILES_1))
LIB_OBJ_FILES := $(addprefix $(OBJDIR)/, $(notdir $(LIB_SRC_FILES:.c=.o)))
LIB_OBJ_FILES += $(LIB_DEPS_OBJ_FILES)

LIB_CFLAGS += $(CFLAGS) -Ilib/ -Ilib/deps/
LIB_LIBS += $(LIBS) $(LDFLAGS) -lm

LIB_DEPS_CFLAGS += $(LIB_CFLAGS) -Wno-misleading-indentation

CONVERTER_HDR_FILES := $(wildcard converter/*.h) $(wildcard converter/*/*.h)
CONVERTER_SRC_FILES := $(wildcard converter/*.c) $(wildcard converter/*/*.c)
CONVERTER_OBJ_FILES := $(addprefix $(OBJDIR)/, \
                         $(notdir $(CONVERTER_SRC_FILES:.c=.o)))

CONVERTER_LIBS += $(LIBS) $(LIB_LIBS)
CONVERTER_CFLAGS += $(CFLAGS) -Iconverter/ -Ilib/

all: tibiarc

tibiarc: library $(CONVERTER_OBJ_FILES)
	$(TRC_CC) $(CFLAGS) $(CONVERTER_OBJ_FILES) -o $@ \
                $(OBJDIR)/libtibiarc.a $(CONVERTER_LIBS)

library: $(OBJDIR)/libtibiarc.a

$(OBJDIR)/libtibiarc.a: $(LIB_OBJ_FILES) $(LIB_DEP_FILES)
	$(TRC_AR) -rcs $@ $(LIB_OBJ_FILES)

$(OBJDIR)/%.o: converter/encoders/%.c
	$(TRC_CC) -c $(CONVERTER_CFLAGS) -MMD -MP -c $< -o $@
$(OBJDIR)/%.o: converter/%.c
	$(TRC_CC) -c $(CONVERTER_CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR)/%.o: lib/deps/%.c
	$(TRC_CC) -c $(LIB_DEPS_CFLAGS) -MMD -MP -c $< -o $@
$(OBJDIR)/%.o: lib/formats/%.c
	$(TRC_CC) -c $(LIB_CFLAGS) -MMD -MP -c $< -o $@
$(OBJDIR)/%.o: lib/%.c
	$(TRC_CC) -c $(LIB_CFLAGS) -MMD -MP -c $< -o $@

-include $(LIB_OBJ_FILES:.o=.d)
-include $(LIB_DEPS_OBJ_FILES:.o=.d)
-include $(CONVERTER_OBJ_FILES:.o=.d)

# ########################################################################### #

.PHONY: analyze
analyze:
	scan-build make

.PHONY: format
format:
	clang-format -i \
            $(LIB_SRC_FILES) $(LIB_HDR_FILES) \
            $(CONVERTER_HDR_FILES) $(CONVERTER_SRC_FILES)

clean:
	(cd $(OBJDIR) && rm -rf *.o *.d)
	(cd tests && make clean)
