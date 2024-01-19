#!/bin/sh

CONVERTER_FILES=`find converter/ -type f`
LIB_FILES=`find lib/ -type f -not -path "lib/deps/*"`

clang-format $1 -i $CONVERTER_FILES $LIB_FILES
