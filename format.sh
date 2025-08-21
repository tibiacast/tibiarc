#!/bin/sh

CONVERTER_FILES=`find converter/ -type f`
COLLATOR_FILES=`find collator/ -type f`
MINER_FILES=`find miner/ -type f -not -path "miner/deps/*"`
GUI_FILES=`find gui/ -type f`
LIB_FILES=`find lib/ -type f -not -path "lib/deps/*"`
UTIL_FILES=`find utils/ -type f`

clang-format $1 -i $CONVERTER_FILES \
                   $COLLATOR_FILES \
                   $MINER_FILES \
                   $GUI_FILES \
                   $LIB_FILES \
                   $UTIL_FILES
