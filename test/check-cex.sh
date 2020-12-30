#!/usr/bin/env bash

TEMP_DIR=$(mktemp -d)
for INPUTF in "$@"
do
  clang -c -emit-llvm "$INPUTF" -o "$TEMP_DIR/$(basename $INPUTF).bc"
done

if command -v llvm-link-9 &> /dev/null
then
  LLVMLINK="llvm-link-9"
  LLI="lli-9"
else
  LLVMLINK="llvm-link"
  LLI="lli"
fi

"$LLVMLINK" "$TEMP_DIR"/*.bc -o - | "$LLI"
RESULT=$?
rm -r "$TEMP_DIR"
exit $RESULT