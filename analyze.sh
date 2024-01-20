#!/bin/sh

mkdir -p _analyze && rm -rf _analyze/*
(cd _analyze && scan-build cmake .. && scan-build make)
