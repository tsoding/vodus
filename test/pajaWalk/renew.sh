#!/bin/sh

VODUS=../../vodus.release

set -xe

rm -rf ./expected-frames/
mkdir -p ./expected-frames/
$VODUS ./pajaWalk.txt expected-frames/ --config ./test.conf
