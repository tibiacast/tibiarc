#!/bin/sh

CONVERTER_FILES=`find converter/ -type f`
PLAYER_FILES=`find player/ -type f`
LIB_FILES=`find lib/ -type f -not -path "lib/deps/*"`

clang-format $1 -i $CONVERTER_FILES $PLAYER_FILES $LIB_FILES
