import subprocess
import argparse
import pathlib
import tempfile

from typing import Tuple

import os
import sys

class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def error_msg(message):
    print(Colors.FAIL + "ERROR: " + Colors.ENDC + message)

def is_executable(file) -> bool:
    '''Returns true if the given file exists and is an executable'''
    if file == None:
        return False
    
    return os.path.isfile(file) and os.access(file, os.X_OK)

def create_temp_dir() -> pathlib.Path:
    return pathlib.Path(tempfile.mkdtemp(prefix="gazer_workdir_"))


class Command:
    def __init__(self, name, help='', depends=[]):
        self.name = name
        self.help = help
        self.depends = depends

    def configure_args(self, parser):
        for cmd in self.depends:
            parser = cmd.configure_args(parser)

        return self.fill_arguments(parser)

    def run(self, args, wd: pathlib.Path) -> Tuple[int, dict]:
        # Execute the commands on which we depend.
        deps = {}
        for cmd in self.depends:
            result = cmd.run(args, wd)
            if result[0] != 0:
                error_msg("{0}: previous command `{1}` exited with an error.".format(self.name, cmd.name))

            deps.update(result[1])

        return self.execute(args, wd, deps)

    def fill_arguments(self, parser):
        raise NotImplementedError

    def execute(self, args, wd: pathlib.Path, deps) -> Tuple[int, dict]:
        raise NotImplementedError

class ClangCommand(Command):
    '''
    Compiles a set of C or C++ files into LLVM bitcode using clang.
    '''
    def __init__(self):
        super().__init__('clang', 'Compile C/C++ sources into LLVM bitcode')

    def fill_arguments(self, parser):
        parser.add_argument("filename", help="Input C file name")
        parser.add_argument('-m', '--machine', type=int,
            dest='machine', choices=[32, 64], help='Machine architecture', default=32)
        parser.add_argument('--clang-path', help='Path to the clang executable')
        parser.add_argument('--clang-flags', nargs='+')

        return parser

    def find_clang(self, args) -> str:
        if args.clang_path != None:
            return args.clang_path
        
        return 'clang'

    def execute(self, args, wd: pathlib.Path, deps) -> Tuple[int, dict]:
        clang_cmd = self.find_clang(args)
        #if not is_executable(clang_cmd):
        #    error_msg("clang executable `{0}` was not found or lacks sufficient permissions.".format(clang_cmd))
        #    return 1

        filename = pathlib.Path(args.filename)
        bc_file = wd.joinpath(filename.with_suffix(".bc").name)

        clang_argv = [
            clang_cmd,
            "-m{0}".format(args.machine),
            "-g",
            # In the newer (>=5.0) versions of clang, -O0 marks functions
            # with a 'not optimizable' flag, which can break the functionality
            # of gazer. Here we request optimizations with -O1 and turn them off
            # immediately by disabling all LLVM passes.
            "-O1", "-Xclang", "-disable-llvm-passes",
            "-c", "-emit-llvm",
            filename,
            "-o", bc_file.absolute()
        ]

        clang_success = subprocess.call(clang_argv)
        if clang_success != 0:
            error_msg("clang exited with a failure.")
            return (1, {})

        return (0, {
            "clang.output_file": bc_file
        })

class GazerBmcCommand(Command):
    '''
    Execute gazer-bmc.
    '''
    def __init__(self):
        super().__init__('bmc', 'Verify program with the built-in bounded model checking engine', depends=[
            ClangCommand()
        ])

    def fill_arguments(self, parser):
        parser.add_argument("-bound", type=int, help="Unwind count", default=100, nargs=None)
        parser.add_argument("-math-int", default=False, action='store_true', help="Represent integers as the arithmetic integer type instead of bitvectors")
        parser.add_argument("-inline", help="Inline non-recursive functions", default=False, action='store_true')
        parser.add_argument("-inline-globals", help="Inline global variables into main", default=False, action='store_true')
        parser.add_argument("-elim-vars", help="Variable elimination level", choices=["off", "normal", "aggressive"], default="normal")
        parser.add_argument("-trace", help="Print counterexample trace", default=False, action='store_true')
        return parser

    def find_gazer_bmc(self, args) -> pathlib.Path:
        #TODO
        return pathlib.Path("tools/gazer-bmc/gazer-bmc")

    def execute(self, args, wd: pathlib.Path, deps) -> Tuple[int, dict]:
        bc_file = deps['clang.output_file']
        gazer_bmc_path = self.find_gazer_bmc(args)

        gazer_argv = [
            gazer_bmc_path,
            "-bmc",
            "-inline", "-inline-globals",
            "-no-optimize",
            "-bound", str(args.bound),
            "-elim-vars={0}".format(args.elim_vars)
        ]

        if args.trace:
            gazer_argv.append("-trace")

        if args.math_int:
            gazer_argv.append("-math-int")

        gazer_argv.append(bc_file)

        gazer_success = subprocess.call(gazer_argv)
        if gazer_success != 0:
            error_msg("gazer-bmc exited with a failure.")
            return (1, {})

        return (0, {})

def run_commands():
    # Register all commands
    commands = {
        'clang': ClangCommand(),
        'bmc': GazerBmcCommand()
    }

    if len(sys.argv) < 2:
        print("USAGE: gazer <command>")
        return 1

    cmdname = sys.argv[1]
    if cmdname not in commands:
        print("Unknown command `{0}`.".format(cmdname))

    current = commands[cmdname]

    parser = argparse.ArgumentParser()
    current.configure_args(parser)

    args = parser.parse_args(sys.argv[2:])

    # Create a temporary working directory
    wd = create_temp_dir()

    result = current.run(args, wd)