#!/bin/sh

CONVERTER_FILES=`find converter/ -type f`
MINER_FILES=`find miner/ -type f -not -path "miner/deps/*"`
PLAYER_FILES=`find player/ -type f`
LIB_FILES=`find lib/ -type f -not -path "lib/deps/*"`
UTIL_FILES=`find utils/ -type f`

clang-format $1 -i $CONVERTER_FILES \
                   $MINER_FILES \
                   $PLAYER_FILES \
                   $LIB_FILES \
                   $UTIL_FILES
