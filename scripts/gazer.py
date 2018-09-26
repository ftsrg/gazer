#!/bin/python
import subprocess
import argparse
import pathlib

parser = argparse.ArgumentParser()
parser.add_argument("filename", help="Input C file name")
parser.add_argument("-unwind", type=int, help="Unwind count", default=10, nargs=None)
parser.add_argument("-verify-cex", help="Verify CEX by running a test harness")
parser.add_argument("-clang-flags", nargs="+")

args = parser.parse_args()

filename = pathlib.Path(args.filename)
bc_file = filename.with_suffix(".bc")

print("Compiling C program into LLVM bitcode.")
clang_success = subprocess.call([
    "clang",
    "-g", "-O1", "-Xclang", "-disable-llvm-passes",
    "-c", "-emit-llvm",
    filename,
    "-o", bc_file
])

if not clang_success == 0:
    print("clang exited with a failure.")
    exit(1)

gazer_bmc_path = pathlib.Path("tools/gazer-bmc/gazer-bmc")

gazer_success = subprocess.call([
    gazer_bmc_path,
    "-inline", "-inline-globals",
    "-bmc", "-optimize", "-trace",
    "-unwind", str(args.unwind),
    "-test-harness=harness.bc",
    bc_file
])

if not gazer_success == 0:
    print("gazer-bmc exited with failure.")
    exit(1)

