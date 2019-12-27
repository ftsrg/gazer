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


class VerifierTool:
    def __init__(self, toolpath: str, file: pathlib.Path, args):
        self.toolpath = toolpath
        self.args = args
        self.file = file

    def call_str(self):
        return "{0} {1} {2}".format(self.toolpath, ' '.join(self.args), self.file.name)

    def success_pattern(self):
        raise NotImplementedError()

    def fail_pattern(self):
        raise NotImplementedError()

class GazerVerifier(VerifierTool):
    def __init__(self, toolpath, file, args):
        super().__init__(toolpath, file, args)

    def success_pattern(self):
        return '"Verification SUCCESSFUL"'

    def fail_pattern(self):
        return '"Verification FAILED"'

class CpaVerifier(VerifierTool):
    def __init__(self, toolpath, file, args):
        super().__init__(toolpath, file, args)

    def success_pattern(self):
        return '"Verification result: TRUE"'

    def fail_pattern(self):
        return '"Verification result: FALSE"'

class Reducer:
    early_reject = ""

    def __init__(self, verif: VerifierTool):
        self.verif = verif

    def generate(self):
        output = ""
        if self.early_reject != "":
            output += "cat {0} | grep '{1}'\n".format(self.verif.file.name, self.early_reject)
        output += self.create_script()

        return output

    def create_script(self) -> str:
        raise NotImplementedError()


class FalseCexReducer(Reducer):
    """Creates an interestingness test targeting an unreproducable counterexample."""
    def __init__(self, verif: VerifierTool, gazer_build_path: pathlib.Path):
        super().__init__(verif)
        self.check_cex = gazer_build_path.joinpath("../test/check-cex.sh").absolute()
        self.errors = gazer_build_path.joinpath("../test/errors.c").absolute()

    def create_script(self) -> str:
        output = self.verif.call_str() + " &> t.log\n"
        output += 'cat t.log | grep {0}\n'.format(self.verif.fail_pattern)
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
    def __init__(self, verif: VerifierTool, safe: VerifierTool):
        super().__init__(verif)
        self.safe = safe

    def create_script(self) -> str:
        output = '{0} | grep {1}\n'.format(self.verif.call_str(), self.verif.fail_pattern())
        output += 'RES1=$?\n'
        output += 'if [ $RES1 -ne 0 ]; then\n'
        output += '    exit 1\n'
        output += 'fi\n'
        output += '{0} | grep {1}\n'.format(self.safe.call_str(), self.verif.success_pattern())
        output += 'RES2=$?\n'
        output += 'if [ $RES2 -ne 0 ]; then\n'
        output += '    exit 1\n'
        output += 'fi\n'

        return output


class InvalidSuccessReducer(Reducer):
    def __init__(self, verif: VerifierTool, safe: VerifierTool):
        super().__init__(verif)
        self.safe = safe

    def create_script(self) -> str:
        output = '{0} | grep {1}\n'.format(self.verif.call_str(), self.verif.success_pattern())
        output += 'RES1=$?\n'
        output += 'if [ $RES1 -ne 0 ]; then\n'
        output += '    exit 1\n'
        output += 'fi\n'
        output += '{0} | grep {1}\n'.format(self.safe.call_str(), self.safe.fail_pattern())
        output += 'RES2=$?\n'
        output += 'if [ $RES2 -ne 0 ]; then\n'
        output += '    exit 1\n'
        output += 'fi\n'

        return output


class OutputPatternReducer(Reducer):
    def __init__(self, verif: VerifierTool, pattern: str):
        super().__init__(verif)
        self.pattern = pattern

    def create_script(self) -> str:
        output = "{0} >& t.log\ngrep '{1}' t.log\n".format(self.verif.call_str(), self.pattern)

        return output

def find_tool(toolname, toolpath) -> pathlib.Path:
    if toolpath != "":
        return pathlib.Path(toolpath).absolute()

    if toolname == "cpa":
        return "cpa"

    return pathlib.Path(os.getcwd()).joinpath("tools/gazer-{0}/gazer-{0}".format(toolname))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('target', choices=['crash', 'false-cex', 'invalid-fail', 'pattern', 'invalid-success'], help='The target behavior to debug')
    parser.add_argument('file', help='The input file to reduce')
    parser.add_argument('--pattern', help='A pattern which must be present in the output')
    parser.add_argument("--early-reject", default="", help="Reject the generated C file if it does not contain the given grep pattern")
    parser.add_argument('--tool', choices=['bmc', 'theta'], default='bmc', help="The gazer tool to debug")
    parser.add_argument('--tool-args', metavar='<tool arguments>', default="", help="Arguments for the gazer tool, enclosed in quotes")
    parser.add_argument('--safe-tool', choices=['bmc', 'theta', 'cpa'], help="Safe tool for comparison in case of false positives and false negatives")
    parser.add_argument('--safe-tool-args', metavar='<safe tool arguments>', default="", help="Arguments for the safe tool, enclosed in quotes")
    parser.add_argument('--tool-path', default="")
    parser.add_argument('--safe-tool-path', default="")

    args = parser.parse_args()

    tool_path = find_tool(args.tool, args.tool_path)
    file_path = pathlib.Path(args.file)
    file_copy = pathlib.Path(os.getcwd()).joinpath(file_path.name).with_suffix(file_path.suffix + "_creduce.c")
    
    tool = None
    if args.tool in ['bmc', 'theta']:
        tool = GazerVerifier(tool_path, file_copy, args.tool_args.split(' '))
    else:
        raise ValueError("Unknown gazer tool!")


    safe_tool_path = None
    if args.safe_tool != "":
        safe_tool_path = find_tool(args.safe_tool, args.safe_tool_path)

    safe_tool = None
    if args.safe_tool in ['bmc', 'theta']:
        safe_tool = GazerVerifier(safe_tool_path, file_copy, args.safe_tool_args.split(' '))
    elif args.safe_tool == 'cpa':
        safe_tool = CpaVerifier(safe_tool_path, file_copy, args.safe_tool_args.split(' '))
    else:
        raise ValueError("Unknown safe tool!")

    reducer = None
    if args.target == 'false-cex':
        tool.args.extend(['-trace', '-test-harness', 'harness.bc'])
        reducer = FalseCexReducer(tool, pathlib.Path(args.gazer_dir).absolute())
    elif args.target == 'invalid-fail':
        reducer = InvalidFailReducer(tool, safe_tool)
    elif args.target == 'invalid-success':
        reducer = InvalidSuccessReducer(tool, safe_tool)
    elif args.target == 'pattern':
        reducer = OutputPatternReducer(tool, args.pattern)
    else:
        raise ValueError("Invalid debug target!")

    if args.early_reject != "":
        reducer.early_reject = args.early_reject

    shutil.copy(args.file, file_copy.as_posix())
    print(reducer.generate())
