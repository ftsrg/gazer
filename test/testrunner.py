import os
import sys
import glob
import logging
import subprocess
import argparse
import pathlib
import operator

class TestCase:
    '''Represents a simple test case which will be run by the test framework'''
    def __init__(self, filename, expected, flags):
        self.filename = filename
        self.expect = expected
        self.flags = flags

class TestParseException(Exception):
    pass

def parse_test(fname):
    '''Searches for test directives in a given file'''
    directives = []
    with open(fname) as file:
        # Try to search for test directives
        for line in file:
            if line.startswith("//@"):
                parts = line.split()
                directive = parts[0][3:]
                directives.append([directive, parts[1:]])
            elif line.strip() == "":
                pass
            else:
                break
    
    # Try to parse the directives
    expect = ""
    flags = []
    for entry in directives:
        dname = entry[0]
        params = entry[1]

        if dname == "expect":
            if expect != "":
                raise TestParseException("Only one 'expect' directive is allowed per file.")

            if params[0] == "success" or params[0] == "fail":
                expect = params[0]
            else:
                raise TestParseException("Invalid value for an 'except' directive.")
        elif dname == "flag":
            flags.append(params)
        else:
            raise TestParseException("Unknown directive: '{0}'" % dname)
    
    if expect == "":
        raise TestParseException("Missing 'expect' directive")
    
    return TestCase(fname, expect, flags)

def discover_tests(directory):
    tests = []
    for (dpath, dnames, fnames) in os.walk(directory):
        # Find all C files
        for file in fnames:
            if not file.endswith(".c"):
                continue
            path = os.path.join(dpath, file)

            try:
                tests.append(parse_test(path))
            except TestParseException as err:
                print("Cannot execute {0}: {1}".format(path, err))
    
    return tests

if __name__ == '__main__':
    results = []

    argument_parser = argparse.ArgumentParser(description="Run Gazer functional test suite.")
    argument_parser.add_argument("tests", help="Path to the test directory.")
    argument_parser.add_argument("--clang-path", default="clang", help="Name of the clang compile command to run")
    argument_parser.add_argument("--gazer-path", help="Path to the Gazer executable.")

    args = argument_parser.parse_args()
    tests = discover_tests(pathlib.Path(args.tests))

    clang_cmd = args.clang_path
    gazer_bmc_path = pathlib.Path(args.gazer_path)

    PASSED = 0
    FAILED = 1
    TIMEOUT = 2
    ERROR = 3
    SKIP = 4
    UNKNOWN = 5

    outputs_file = open("test_outputs.txt", "w")

    for test in sorted(tests, key=operator.attrgetter('filename')):
        # Execute all tests
        print("Test {0} {1}".format(test.filename, test.flags))
        logging.debug("Compiling C program into LLVM bitcode")

        bc_file = pathlib.Path(test.filename).with_suffix(".bc")

        try:
            clang_success = subprocess.run([
                clang_cmd,
                "-Wno-everything",
                "-g", "-O1", "-Xclang", "-disable-llvm-passes",
                "-c", "-emit-llvm",
                test.filename,
                "-o", bc_file
            ])
        except subprocess.CalledProcessError as err:
            results.append(tuple((test.filename, SKIP, "clang exited with a failure")))
            continue

        try:
            gazer_flags = [
                gazer_bmc_path,
                "-inline",
                "-bmc",
                "-no-optimize",
                bc_file
            ]
            for flag in test.flags:
                gazer_flags.extend(flag)

            gazer_output = subprocess.run(
                gazer_flags,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True).stdout

            outputs_file.write(">>>>>> Output for {0} {1}:\n".format(test.filename, test.flags))
            outputs_file.write(gazer_output)

            verification_result = ""
            if "Verification FAILED" in gazer_output:
                verification_result = "fail"
            elif "Verification SUCCESSFUL" in gazer_output:
                verification_result = "success"

            if verification_result == "":
                results.append(tuple((test.filename, UNKNOWN, "Unknown value returned by gazer")))
            elif verification_result == test.expect:
                results.append(tuple((test.filename, PASSED, "")))
            else:
                results.append(tuple((test.filename, FAILED, "Expected: {0} Actual: {1}".format(test.expect, verification_result))))
        except subprocess.CalledProcessError as err:
            results.append(tuple((test.filename, ERROR, "gazer exited with an error")))
            continue

    print("\n=========================")
    print("-------- RESULTS --------")
    print("=========================\n")
    for result in results:
        if result[1] == PASSED:
            print("PASS    {0}".format(result[0]))
        elif result[1] == FAILED:
            print("FAIL    {0} ({1})".format(result[0], result[2]))
        elif result[1] == ERROR:
            print("ERROR   {0} ({1})".format(result[0], result[2]))
        elif result[1] == SKIP:
            print("SKIP    {0} ({1})".format(result[0], result[2]))
        else:
            print("UNKNOWN {0} ({1})".format(result[0], result[2]))
        
