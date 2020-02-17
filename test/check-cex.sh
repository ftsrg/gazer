#!/usr/bin/env bash

TEMP_DIR=$(mktemp -d)
for INPUTF in "$@"
do
  clang -c -emit-llvm "$INPUTF" -o "$TEMP_DIR/$(basename $INPUTF).bc"
done

llvm-link "$TEMP_DIR"/*.bc -o - | lli
RESULT=$?
rm -r "$TEMP_DIR"
exit $RESULT