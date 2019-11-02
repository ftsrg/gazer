#!/usr/bin/env python3
# ==- testreduce.py - Test case reducer ----------------------*- python -*--===//
#
#  Copyright 2019 Contributors to the Gazer project
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
# ===----------------------------------------------------------------------===//
"""This script generates C-Reduce interestingness tests to facilitate debugging."""

import argparse
import pathlib
import shutil
import os


class VerifierConfig:
    def __init__(self, tool, file, flags):
        self.tool = tool
        self.file = file
        self.flags = flags

    def call_str(self):
        return "{0} {1} {2}".format(self.tool, ' '.join(self.flags), self.file)


class Reducer:
    early_reject = ""

    def __init__(self, verif: VerifierConfig):
        self.verif = verif

    def generate(self):
        output = ""
        if self.early_reject != "":
            output += "cat {0} | grep '{1}'\n".format(self.verif.file, self.early_reject)
        output += self.create_script()

        return output

    def create_script(self) -> str:
        raise NotImplementedError()


class FalseCexReducer(Reducer):
    """Creates an interestingness test targeting an unreproducable counterexample."""
    def __init__(self, verif: VerifierConfig, gazer_build_path: pathlib.Path):
        super().__init__(verif)
        self.check_cex = gazer_build_path.joinpath("../test/check-cex.sh").absolute()
        self.errors = gazer_build_path.joinpath("../test/errors.c").absolute()

    def create_script(self) -> str:
        output = self.verif.call_str() + " &> t.log\n"
        output += 'cat t.log | grep "Verification FAILED"\n'
        output += 'RESULT=$?\n'
        output += 'if [ $RESULT -ne 0 ]; then\n'
        output += '    exit 1\n'
        output += 'fi\n'
        output += '! {0} {1} {2} {3} | grep "executed"\n'.format(
            self.check_cex,
            self.errors,
            self.verif.file,
            "harness.bc"
        )

        return output


class InvalidFailReducer(Reducer):
    def __init__(self, verif: VerifierConfig, safe: VerifierConfig):
        super().__init__(verif)
        self.safe = safe

    def create_script(self) -> str:
        output = self.verif.call_str() + ' | grep "FAILED"\n'
        output += 'RES1=$?\n'
        output += 'if [ $RES1 -ne 0 ]; then\n'
        output += '    exit 1\n'
        output += 'fi\n'
        output += self.safe.call_str() + ' | grep "SUCCESSFUL"\n'
        output += 'RES2=$?\n'
        output += 'if [ $RES2 -ne 0 ]; then\n'
        output += '    exit 1\n'
        output += 'fi\n'

        return output


class OutputPatternReducer(Reducer):
    def __init__(self, verif: VerifierConfig, pattern: str):
        super().__init__(verif)
        self.pattern = pattern

    def create_script(self) -> str:
        output = "{0} >& t.log\ngrep '{1}' t.log\n".format(self.verif.call_str(), self.pattern)

        return output


def find_tool(toolname, gazer_dir):
    if toolname == "bmc":
        return pathlib.Path(gazer_dir).joinpath("tools/gazer-bmc/gazer-bmc")
    elif toolname == "theta":
        return pathlib.Path(gazer_dir).joinpath("tools/gazer-theta/gazer-theta")

    raise ValueError("Unknown toolname")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('target', choices=['crash', 'false-cex', 'invalid-fail', 'pattern'], help='The target behavior to debug')
    parser.add_argument('file', help='The input file to reduce')
    parser.add_argument('--pattern', help='A pattern which must be present in the output')
    parser.add_argument('--gazer-dir', default=os.getcwd())
    parser.add_argument("--early-reject", default="", help="Reject the generated C file if it does not contain the given grep pattern")
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

        reducer = FalseCexReducer(config, pathlib.Path(args.gazer_dir).absolute())
        print(reducer.generate())
    elif args.target == 'invalid-fail':
        safe_tool = find_tool(args.safe_tool, args.gazer_dir)

        fcopy = pathlib.Path(args.file).name + "_credue.c"
        shutil.copy(args.file, fcopy)

        config = VerifierConfig(tool, pathlib.Path(fcopy).name, args.tool_args.split(' '))
        safe_config = VerifierConfig(safe_tool, pathlib.Path(fcopy).name, args.safe_tool_args.split(' '))

        reducer = InvalidFailReducer(config, safe_config)
        print(reducer.generate())
    elif args.target == 'pattern':
        fcopy = pathlib.Path(args.file).name + "_credue.c"
        shutil.copy(args.file, fcopy)

        config = VerifierConfig(tool, pathlib.Path(fcopy).name, args.tool_args.split(' '))

        reducer = OutputPatternReducer(config, args.pattern)
        reducer.early_reject = args.early_reject
        print(reducer.generate())

