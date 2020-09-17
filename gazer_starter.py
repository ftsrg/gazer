#!/usr/bin/python3

# This script runs Gazer and Theta with several configurations on the given programfile
# test harnesses (counterexamples) and witnesses in the format of SV-Comp '21 are generated

import re
import os
import errno
import signal
import psutil
import sys
import subprocess
import re
import hashlib
import argparse
import enum
from subprocess import Popen, PIPE, TimeoutExpired
from time import monotonic as timer

# TODO tweak timeouts, should the cartpred config get a timeout?
bmc_timeout = 200.0 # timeout of bmc run
theta_explicit_timeout = 200.0 # timeout of explicit theta run
test_harness_compile_timeout = 50.0 # timeout of compiling test harness/counterexample
counterexample_run_timeout = 100.0 # timeout of running the generated counterexample

theta_explicit_config = ["--search ERR", "--domain EXPL", "--maxenum 100", "--refinement BW_BIN_ITP", "--initprec ALLVARS"]
theta_cartpred_config = ["--inline all", "--search ERR", "--domain PRED_CART", "--refinement BW_BIN_ITP", "--initprec EMPTY"]
bmc_config = ["--inline all", "--bound 1000000"] # bound: We'll kill it after the timeout anyway, so it can be really big, why not

# default values
output_path = os.getcwd()
tool_directory = os.path.abspath(os.path.dirname(__file__) + "/tools") # gazer/tools

class Result(enum.Enum):
    UNKNOWN = 1
    ERROR = 2
    TRUE = 3
    FALSE = 4

result = Result.UNKNOWN # should be printed with result.name

def kill():
    parent = psutil.Process(os.getpid())
    for child in parent.children(recursive=True):  # or parent.children() for recursive=False
        child.kill()

def print_bmc():
    print("gazer-bmc:")

def print_theta():
    print("gazer-theta:")

def print_line():
    print("\n------------------------------------------\n")

def print_result():
    print("Result of gazer-theta run: " + result.name)

# If output is None, prints "No output"
def print_if_not_empty(output):
    if(output != None): print(output.decode('utf-8'))
    else: print("No output")

# raises CalledProcessError, if returncode isn't 0; raises TimeoutExpire, if process times out
def run_with_timeout(command, timeout, env=None, no_print=False):
    # instead of subprocess.check_output()
    # to enforce timeout before Python 3.7.5
    # and kill sub-processes to avoid interference
    # https://stackoverflow.com/a/36955420
    start = timer()
    with subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True, env=env) as process:
        try:
            # note: stderr is forwarded to stdout and they are handled together, so we won't use stderr_dummy
            stdout_bytes, stderr_dummy = process.communicate(input=None, timeout=timeout)
            returncode = process.poll()

            if returncode:
                if(not no_print): print('Command returned with ' + str(returncode) + ' after {:.2f} seconds'.format(timer()-start))
                raise subprocess.CalledProcessError(returncode, process.args, output=stdout_bytes)
            else:
                if(not no_print): print('Command returned with 0 after {:.2f} seconds'.format(timer() - start))
                return stdout_bytes
        except KeyboardInterrupt:
            os.killpg(process.pid, signal.SIGINT)  # send signal to the process group
            raise KeyboardInterrupt
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGINT)  # send signal to the process group
            # stdout_bytes, stderr_dummy = process.communicate() - hangs sometimes
            if(not no_print): print('Command timeout after {:.2f} seconds'.format(timer() - start))
            raise TimeoutExpired(process.args, timeout, output=None)

# raises CalledProcessError, if returncode isn't 0; no timeout - waits for the process to return infinitely
def run_without_timeout(command, env=None, no_print=False):
    # instead of subprocess.check_output()
    # to enforce timeout before Python 3.7.5
    # and kill sub-processes to avoid interference
    # https://stackoverflow.com/a/36955420
    start = timer()
    with subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True, env=env) as process:
        try:
            # note: stderr is forwarded to stdout and they are handled together, so we won't use stderr_dummy
            stdout_bytes, stderr_dummy = process.communicate(input=None)
            returncode = process.poll()

            if returncode:
                if(not no_print): print('Command returned with ' + str(returncode) + ' after {:.2f} seconds'.format(timer()-start))
                raise subprocess.CalledProcessError(returncode, process.args, output=stdout_bytes)
            else:
                if(not no_print): print('Command returned with 0 after {:.2f} seconds'.format(timer() - start))
                return stdout_bytes
        except KeyboardInterrupt:
            os.killpg(process.pid, signal.SIGINT)  # send signal to the process group
            raise KeyboardInterrupt
        
# changes the global result parameter based on output of the given bmc/theta run
# tries to run the test harness, if needed
# should be given an output from a run with return code 0
def parse_output(output, task):
    # parses in reverse, as the output can be long, but the lines searched are around the end (mostly the last lines)
    for line in reversed(output.decode('utf-8').split('\n')):
        global result
        if "Verification FAILED" in line:
            result = run_test_harness(task) # FALSE, ERROR or UNKNOWN
        if "Verification SUCCESSFUL" in line:
            result = Result.TRUE
        if "Verification BOUND REACHED" in line:
            result = Result.UNKNOWN
        if "Verification INTERNAL ERROR" in line:
            result = Result.ERROR

# We know about the run that it was successful, but if it has a false result, we should run
# the generated test harness as well to make sure, that the counterexample is correct
def run_test_harness(task): # TODO ide is kellhet -m32, ha a Gazerben is lesz
    task_name = os.path.basename(task)
    # clang task harness.ll -o task_test
    # ./task_test -> if it outputs Aborted return True, if not return False
    clang_command = "clang " + task + " " + output_path + "/" + task_name + ".ll -o " + output_path + "/" + task_name + "_test"
    test_command = output_path + "/" + task_name + "_test"

    print("Running " + clang_command + "\n")
    try:
        clang_output = run_with_timeout(clang_command, test_harness_compile_timeout)
        print_if_not_empty(clang_output)
    except subprocess.CalledProcessError as err:
        print("Could not compile test harness")
        return Result.ERROR
    except subprocess.TimeoutExpired as err:
        print("Could not compile test harness, timed out after " + str(err.timeout))
        return Result.ERROR

    print("Running " + test_command + "\n")
    try:
        test_output = run_with_timeout(test_command, counterexample_run_timeout)
        print_if_not_empty(test_output)
        print("Counterexample is wrong, result is unknown")
        return Result.UNKNOWN
    except subprocess.CalledProcessError as err:
        good_output = 'reach_error: Assertion `0\' failed'
        if(good_output in err.output.decode('utf-8')):
            print("Counterexample ok")
            return Result.FALSE
        else:
            print("Counterexample returned with " + str(err.returncode))
            print("Counterexample is wrong, result is unknown")
            return Result.UNKNOWN
    except subprocess.TimeoutExpired as err:
        print("Counterexample timed out after " + str(err.timeout))
        return Result.ERROR

# if timeout is 0, then it will be run without timeout
def run_next_config(toolname, flags, task_with_path, timeout):
    print_line()

    command = tool_directory + toolname + " " + ' '.join(flags) + " " + task_with_path
    print("Running " + command + "\n")
    if(timeout!=0):
        try:
            output = run_with_timeout(command, timeout)
            print_if_not_empty(output)
            if(output != None):
                parse_output(output, task_with_path)
            print_line()
        except subprocess.CalledProcessError as err:
            print_if_not_empty(err.output)
        except subprocess.TimeoutExpired as err:
            print_if_not_empty(err.output)
    else:
        try:
            output = run_without_timeout(command)
            print_if_not_empty(output)
            if(output != None):
                parse_output(output, task_with_path)
            print_line()
        except subprocess.CalledProcessError as err:
            print_if_not_empty(err.output)
        
def get_version_number():
    z3path = tool_directory + "/gazer-theta/theta/lib"
    gazer_version = run_without_timeout(tool_directory + "/gazer-theta/gazer-theta --version", no_print=True)
    theta_ver = run_without_timeout("java -Djava.library.path=" + z3path + " -jar " + tool_directory + "/gazer-theta/theta/theta-cfa-cli.jar --version", env={"LD_LIBRARY_PATH": z3path}, no_print=True)
    return "Theta v" + theta_ver.decode('utf-8') + re.search(r"Gazer v\d*[.]\d*[.]\d*", gazer_version.decode('utf-8')).group()


def main():
    global output_path

    parser = argparse.ArgumentParser()

    parser.add_argument("--version", action="version", version=get_version_number())
    parser.add_argument("task", help="name of the to be verified programfile with path")
    parser.add_argument("--output", help="output directory (for the witness and test harness), default: working directory")

    args = parser.parse_args()
    print("Parsing arguments done")
    
    # initialize argument values
    taskname = os.path.basename(args.task)
    task_with_path = os.path.abspath(args.task)

    if args.output != None:
        output_path = os.path.abspath(args.output)

    # check if the output directory exists, create it if not
    if(not os.path.exists(output_path) or not os.path.isdir(output_path)):
        try:
            os.makedirs(output_path)
        except OSError:
            print ("Creation of the directory %s failed" % output_path)
            return
        else:
            print ("Successfully created the directory %s" % output_path)

    # check, if the tool directory is correct and has gazer-bmc and gazer-theta as well
    if(not os.path.isfile(tool_directory+"/gazer-bmc/gazer-bmc")):
        raise FileNotFoundError(errno.ENOENT, os.strerror(errno.ENOENT), tool_directory+"/gazer-bmc/gazer-bmc")
    if(not os.path.isfile(tool_directory+"/gazer-theta/gazer-theta")):
        raise FileNotFoundError(errno.ENOENT, os.strerror(errno.ENOENT), tool_directory+"/gazer-theta/gazer-theta")

    print("Hashing taskfile " + taskname)
    with open(task_with_path, 'r') as pgf:
        hash_of_source = hashlib.sha256(pgf.read().encode('utf-8')).hexdigest()
    print("Hashing done")

    outputfile_flags = ["--witness " + output_path + "/" + taskname + ".witness.graphml", "--hash " + hash_of_source, "--trace", "-test-harness=" + output_path + "/" + taskname + ".ll"]

    print_line()
    print("Starting combined gazer-theta-bmc run...")
    
    run_next_config("/gazer-bmc/gazer-bmc", bmc_config + outputfile_flags, task_with_path, bmc_timeout)
    if(result == Result.FALSE or result == Result.TRUE):
        print_result()
        return

    run_next_config("/gazer-theta/gazer-theta", theta_explicit_config + outputfile_flags, task_with_path, theta_explicit_timeout)
    if(result == Result.FALSE or result == Result.TRUE):
        print_result()
        return

    run_next_config("/gazer-theta/gazer-theta", theta_cartpred_config + outputfile_flags, task_with_path, 0)

    print_result()

if __name__ == "__main__":
    main()
