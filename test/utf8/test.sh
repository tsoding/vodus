#!/usr/bin/env sh

VODUS=../../vodus.release
DIFFIMG=../../diffimg

set -xe

rm -rf ./actual-frames/
mkdir -p ./actual-frames/
$VODUS ./utf-8.txt actual-frames/ --config ./test.conf

EXPECTED_COUNT=`ls ./expected-frames/ | wc -l`
ACTUAL_COUNT=`ls ./actual-frames/ | wc -l`

if [ "$EXPECTED_COUNT" != "$ACTUAL_COUNT" ]; then
    echo "UNEXPECTED AMOUNT OF FRAMES!"
    echo "Expected: $EXPECTED_COUNT"
    echo "Actual:   $ACTUAL_COUNT"
    exit 1
fi

for frame in `ls ./expected-frames/`; do
    $DIFFIMG -e "./expected-frames/$frame" -a "./actual-frames/$frame" -t 5
done
