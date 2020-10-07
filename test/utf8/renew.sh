#!/bin/sh

VODUS=../../vodus.release

set -xe

rm -rf ./expected-frames/
mkdir -p ./expected-frames/
$VODUS ./utf-8.txt expected-frames/ --config ./test.conf
