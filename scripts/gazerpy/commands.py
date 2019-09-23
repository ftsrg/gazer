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
    print(Colors.BOLD + Colors.FAIL + "error: " + Colors.ENDC + message)

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
                return (1, {})

            deps.update(result[1])

        return self.execute(args, wd, deps)

    def fill_arguments(self, parser):
        raise NotImplementedError

    def execute(self, args, wd: pathlib.Path, deps) -> Tuple[int, dict]:
        raise NotImplementedError

class ClangCommand(Command):
    '''Compiles a set of C or C++ files into LLVM bitcode using clang.'''
    def __init__(self):
        super().__init__('clang', 'Compile C/C++ sources into LLVM bitcode')

    def fill_arguments(self, parser):
        parser.add_argument("inputs", metavar='FILE', help="Input file name", nargs='+')
        parser.add_argument('-m', '--machine', type=int,
            dest='machine', choices=[32, 64], help='Machine architecture', default=32)
        parser.add_argument('-o', dest='output_file')
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

        bc_files = []

        for inputf in args.inputs:
            filename = pathlib.Path(inputf)
            extension = filename.suffix

            if extension == '.bc' or extension == '.ll':
                bc_files.append(filename)
                continue
            elif extension != '.c':
                error_msg("Cannot compile source file '{0}'. Supported extensions are: .c, .bc, .ll".format(filename))
                return (1, {})

            clang_bc_file = wd.joinpath(filename.with_suffix(".bc").name)

            try:
                self.clang_compile(filename, clang_bc_file, clang_cmd, args.machine)
                bc_files.append(clang_bc_file)
            except RuntimeError as ex:
                error_msg(str(ex))
                return (1, {})

        result_file = None
        if args.output_file != None:
            result_file = pathlib.Path(args.output_file)
        else:
            result_file = wd.joinpath('gazer_llvm_output.bc')

        try:
            self.llvm_link_files(bc_files, result_file)
        except RuntimeError as ex:
            error_msg(str(ex))
            return (1, {})

        return (0, {
            "clang.output_file": result_file
        })

    def llvm_link_files(self, bc_files, output_file):
        llvm_link_cmd = 'llvm-link'
        
        llvm_link_argv = [
            llvm_link_cmd,
            "-o", output_file.absolute()
        ]

        llvm_link_argv.extend(bc_files)

        llvm_link_success = subprocess.call(llvm_link_argv)
        if llvm_link_success != 0:
            raise RuntimeError("llvm-link exited with a failure")


    def clang_compile(self, input_file, output_file, clang_cmd, machine):
        clang_argv = [
            clang_cmd,
            # TODO: Having some issues with this setting. Somewhy LLVM
            # replaces error calls with indirect ones if -m32 is supplied.
            # "-m{0}".format(machine),
            "-g",
            # In the newer (>=5.0) versions of clang, -O0 marks functions
            # with a 'not optimizable' flag, which can break the functionality
            # of gazer. Here we request optimizations with -O1 and turn them off
            # immediately by disabling all LLVM passes.
            "-O1", "-Xclang", "-disable-llvm-passes",
            "-c", "-emit-llvm",
            input_file,
            "-o", output_file.absolute()
        ]
        
        clang_success = subprocess.call(clang_argv)
        if clang_success != 0:
            raise RuntimeError("clang exited with a failure")

def find_gazer_tools_dir() -> pathlib.Path:
    gazer_tools_dir = None

    env_path = os.getenv('GAZER_TOOLS_DIR')
    if env_path != None:
        gazer_tools_dir = pathlib.Path(env_path)
    else:
        # TODO: This should be something (much) more flexible
        gazer_tools_dir = pathlib.Path("tools")

    if not gazer_tools_dir.exists():
        raise IOError("Could not find gazer_tools_dir!")

    return gazer_tools_dir

def add_gazer_llvm_pass_args(parser):
    parser.add_argument("-inline", help="Inline non-recursive functions", default=False, action='store_true')
    parser.add_argument("-inline-globals", help="Inline global variables into main", default=False, action='store_true')
    parser.add_argument("-no-optimize", help="Disable non-crucial LLVM optimization passes.", default=False, action='store_true')

def add_gazer_frontend_args(parser):
    parser.add_argument("-elim-vars", help="Variable elimination level", choices=["off", "normal", "aggressive"], default="normal")
    parser.add_argument("-math-int", default=False, action='store_true', help="Represent integers as the arithmetic integer type instead of bitvectors")
    parser.add_argument("-no-simplify-expr", default=False, action='store_true', help="Do not simplify expressions")

class GazerBmcCommand(Command):
    '''
    Execute gazer-bmc.
    '''
    def __init__(self):
        super().__init__('bmc', 'Verify program with the built-in bounded model checking engine', depends=[
            ClangCommand()
        ])

    def fill_arguments(self, parser):
        add_gazer_llvm_pass_args(parser)
        add_gazer_frontend_args(parser)
        parser.add_argument("-bound", type=int, help="Unwind count", default=100, nargs=None)
        parser.add_argument("-trace", help="Print counterexample trace", default=False, action='store_true')
        parser.add_argument("-test-harness", help="Test harness file")
        return parser

    def find_gazer_bmc(self, args) -> pathlib.Path:
        gazer_tools_dir = find_gazer_tools_dir()
        return gazer_tools_dir.joinpath("gazer-bmc/gazer-bmc")

    def execute(self, args, wd: pathlib.Path, deps) -> Tuple[int, dict]:
        bc_file = deps['clang.output_file']
        gazer_bmc_path = self.find_gazer_bmc(args)

        gazer_argv = [
            gazer_bmc_path,
            "-bound", str(args.bound),
            "-elim-vars={0}".format(args.elim_vars)
        ]

        if args.inline:
            gazer_argv.append("-inline")

        if args.inline_globals:
            gazer_argv.append("-inline-globals")

        if args.trace:
            gazer_argv.append("-trace")

        if args.math_int:
            gazer_argv.append("-math-int")

        if args.no_optimize:
            gazer_argv.append("-no-optimize")

        if args.test_harness != None:
            gazer_argv.append("-test-harness={0}".format(args.test_harness))

        gazer_argv.append(bc_file)

        gazer_success = subprocess.call(gazer_argv)
        if gazer_success != 0:
            error_msg("gazer-bmc exited with a failure.")
            return (1, {})

        return (0, {})

class PrintCfaCommand(Command):
    def __init__(self):
        super().__init__('print-cfa', 'Print gazer CFA to the standard output', depends=[
            ClangCommand()
        ])

    def fill_arguments(self, parser):
        add_gazer_frontend_args(parser)
        return parser

    def find_gazer_cfa(self, args) -> pathlib.Path:
        gazer_tools_dir = find_gazer_tools_dir()
        return gazer_tools_dir.joinpath("gazer-cfa/gazer-cfa")

    def execute(self, args, wd: pathlib.Path, deps) -> Tuple[int, dict]:
        bc_file = deps['clang.output_file']
        gazer_cfa_path = self.find_gazer_cfa(args)

        gazer_argv = [
            gazer_cfa_path,
            "-elim-vars={0}".format(args.elim_vars)
        ]

        if args.no_simplify_expr:
            gazer_argv.append("-no-simplify-expr")

        if args.math_int:
            gazer_argv.append("-math-int")

        gazer_argv.append(bc_file)

        gazer_success = subprocess.call(gazer_argv)
        if gazer_success != 0:
            error_msg("gazer-cfa exited with a failure.")
            return (1, {})

        return (0, {})

def run_commands():
    # Register all commands
    commands = {
        'clang': ClangCommand(),
        'bmc': GazerBmcCommand(),
        'print-cfa': PrintCfaCommand()
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

    current.run(args, wd)