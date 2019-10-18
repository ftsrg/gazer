#!/usr/bin/env python3
"""This script generates C-Reduce interestingness tests to facilitate debugging."""

import argparse
import pathlib
import tempfile
import shutil
import os

class VerifierConfig:
    def __init__(self, tool, file, flags):
        self.tool = tool
        self.file = file
        self.flags = flags

    def call_str(self):
        return "{0} {1} {2}".format(self.tool, ' '.join(self.flags), self.file)


def create_false_cex_test(verif: VerifierConfig, gazer_build_path: pathlib.Path):
    """Creates an interestingness test targeting an unreproducable counterexample."""
    check_cex = gazer_build_path.joinpath("../test/check-cex.sh").absolute()
    errors = gazer_build_path.joinpath("../test/errors.c").absolute()

    output = verif.call_str() + " &> t.log\n"
    output += 'cat t.log | grep "Verification FAILED"\n'
    output += 'RESULT=$?\n'
    output += 'if [ $RESULT -ne 0 ]; then\n'
    output += '    exit 1\n'
    output += 'fi\n'
    output += '! {0} {1} {2} {3} | grep "executed"\n'.format(check_cex, errors, verif.file, "harness.bc")

    return output

def create_invalid_fail_test(verif: VerifierConfig, safe: VerifierConfig):
    output = verif.call_str() + ' | grep "FAILED"\n'
    output += 'RES1=$?\n'
    output += 'if [ $RES1 -ne 0 ]; then\n'
    output += '    exit 1\n'
    output += 'fi\n'
    output += safe.call_str() + ' | grep "SUCCESSFUL"\n'
    output += 'RES2=$?\n'
    output += 'if [ $RES2 -ne 0 ]; then\n'
    output += '    exit 1\n'
    output += 'fi\n'

    return output

def find_tool(toolname, gazer_dir):
    if toolname == "bmc":
        return pathlib.Path(gazer_dir).joinpath("tools/gazer-bmc/gazer-bmc")
    elif toolname == "theta":
        return pathlib.Path(gazer_dir).joinpath("tools/gazer-theta/gazer-theta")

    raise ValueError("Unknown toolname")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('target', choices=['crash', 'false-cex', 'invalid-fail'], help='The target behavior to debug')
    parser.add_argument('file', help='The input file to reduce')
    parser.add_argument('--gazer-dir', default=os.getcwd())
    parser.add_argument('--tool', choices=['bmc', 'theta'], default='bmc')
    parser.add_argument('--tool-args', metavar='<tool arguments>', default="")
    parser.add_argument('--safe-tool', choices=['bmc', 'theta', 'cpa'])
    parser.add_argument('--safe-tool-args', metavar='<safe tool arguments>', default="")

    args = parser.parse_args()

    tool = find_tool(args.tool, args.gazer_dir)


    if args.target == 'false-cex':
        fcopy = pathlib.Path(args.file).name + "_credue.c"
        shutil.copy(args.file, fcopy)

        config = VerifierConfig(tool, pathlib.Path(fcopy).absolute(), args.tool_args.split(' '))
        config.flags.extend(['-trace', '-test-harness', 'harness.bc'])
        print(create_false_cex_test(config, pathlib.Path(args.gazer_dir).absolute()))
    elif args.target == 'invalid-fail':
        safe_tool = find_tool(args.safe_tool, args.gazer_dir)

        fcopy = pathlib.Path(args.file).name + "_credue.c"
        shutil.copy(args.file, fcopy)

        config = VerifierConfig(tool, pathlib.Path(fcopy).name, args.tool_args.split(' '))
        safe_config = VerifierConfig(safe_tool, pathlib.Path(fcopy).name, args.safe_tool_args.split(' '))
        print(create_invalid_fail_test(config, safe_config))
