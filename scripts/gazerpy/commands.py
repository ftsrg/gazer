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
        super().__init__('clang', 'Compile and link C/C++ or LLVM IR sources into a single LLVM IR module')

    def fill_arguments(self, parser):
        parser.add_argument("inputs", metavar='FILE', help="Input file names", nargs='+')
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
    parser.add_argument("-show-final-cfg", help="Display the CFG after optimization", default=False, action='store_true')

def add_gazer_frontend_args(parser):
    parser.add_argument("-elim-vars", help="Variable elimination level", choices=["off", "normal", "aggressive"], default="normal")
    parser.add_argument("-math-int", default=False, action='store_true', help="Represent integers as the arithmetic integer type instead of bitvectors")
    parser.add_argument("-no-simplify-expr", default=False, action='store_true', help="Do not simplify expressions")

def handle_gazer_llvm_args(args, gazer_argv):
    if args.inline:
        gazer_argv.append("-inline")

    if args.inline_globals:
        gazer_argv.append("-inline-globals")

    if args.no_optimize:
        gazer_argv.append("-no-optimize")

    if args.show_final_cfg:
        gazer_argv.append("-show-unrolled-cfg")

def handle_gazer_frontend_args(args, gazer_argv):
    gazer_argv.append("-elim-vars={0}".format(args.elim_vars))

    if args.math_int:
        gazer_argv.append("-math-int")

    if args.no_simplify_expr:
        gazer_argv.append("-no-simplify-expr")

class GazerBmcCommand(Command):
    '''Executes gazer-bmc.'''
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
        ]

        handle_gazer_llvm_args(args, gazer_argv)
        handle_gazer_frontend_args(args, gazer_argv)

        if args.trace:
            gazer_argv.append("-trace")

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
        add_gazer_llvm_pass_args(parser)
        parser.add_argument("-view", help="View CFA", default=False, action='store_true')
        parser.add_argument("-cyclic", help="Represent loops as cycles instead of recursion.", action='store_true')

        return parser

    def find_gazer_cfa(self, args) -> pathlib.Path:
        gazer_tools_dir = find_gazer_tools_dir()
        return gazer_tools_dir.joinpath("gazer-cfa/gazer-cfa")

    def execute(self, args, wd: pathlib.Path, deps) -> Tuple[int, dict]:
        bc_file = deps['clang.output_file']
        gazer_cfa_path = self.find_gazer_cfa(args)

        gazer_argv = [
            gazer_cfa_path
        ]

        if args.view:
            gazer_argv.append("-view-cfa")

        if args.cyclic:
            gazer_argv.append("-cyclic")

        handle_gazer_llvm_args(args, gazer_argv)
        handle_gazer_frontend_args(args, gazer_argv)
        gazer_argv.append(bc_file)

        gazer_success = subprocess.call(gazer_argv)
        if gazer_success != 0:
            error_msg("gazer-cfa exited with a failure.")
            return (1, {})

        return (0, {})

class ThetaCommand(Command):
    '''Executes gazer-bmc.'''
    def __init__(self):
        super().__init__('theta', 'Verify program using the theta verifier', depends=[
            ClangCommand()
        ])

    def fill_arguments(self, parser):
        add_gazer_llvm_pass_args(parser)
        add_gazer_frontend_args(parser)
        parser.add_argument("--theta-path", help="Path to the theta CFA tool jar.")
        parser.add_argument("--z3-path", help="Path to the Z3 object files.")
        parser.add_argument("--domain", help="Abstract domain", default="PRED_CART", choices=["EXPL", "PRED_BOOL", "PRED_CART", "PRED_SPLIT"])
        parser.add_argument("--encoding", help="Block encoding", default="LBE", choices=["SBE", "LBE"])
        parser.add_argument("--initprec", help="Initial precision of abstraction", default="EMPTY", choices=["EMPTY", "ALLVARS"])
        parser.add_argument("--theta-log", help="Detailedness of logging", default="SUBSTEP", choices=["RESULT", "MAINSTEP", "SUBSTEP", "INFO", "DETAIL", "VERBOSE"])
        parser.add_argument("--precgranularity", help="Precision granularity", default="GLOBAL", choices=["GLOBAL", "LOCAL"])
        parser.add_argument("--predsplit", help="Predicate splitting (for predicate abstraction)", default="WHOLE", choices=["WHOLE", "CONJUNCTS", "ATOMS"])
        parser.add_argument("--refinement", help="Refinement strategy", default="SEQ_ITP", choices=["FW_BIN_ITP", "BW_BIN_ITP", "SEQ_ITP", "MULTI_SEQ", "UNSAT_CORE"])
        parser.add_argument("--search", help="Search strategy", default="BFS", choices=["BFS", "DFS", "ERR"])
        parser.add_argument("--maxenum", default="0")
        parser.add_argument("-trace", help="Print counterexample trace", default=False, action='store_true')
        return parser

    def find_gazer_theta(self, args) -> pathlib.Path:
        gazer_tools_dir = find_gazer_tools_dir()
        return gazer_tools_dir.joinpath("gazer-theta/gazer-theta")

    def find_theta(self, args) -> pathlib.Path:
        if args.theta_path != None:
            return pathlib.Path(args.theta_path)
        
        if os.getenv("THETA_CFA_PATH") != None:
            return pathlib.Path(os.getenv("THETA_CFA_PATH"))

        return None

    def execute(self, args, wd: pathlib.Path, deps) -> Tuple[int, dict]:
        bc_file = deps['clang.output_file']

        gazer_theta_path = self.find_gazer_theta(args)
        theta_cfa_path = self.find_theta(args)

        if theta_cfa_path == None:
            error_msg("Could not find the theta-cfa tool.")
            return (1, {})

        theta_input = wd.joinpath('gazer_theta_cfa.theta')

        gazer_argv = [
            gazer_theta_path,
            "-o", theta_input
        ]

        handle_gazer_llvm_args(args, gazer_argv)
        handle_gazer_frontend_args(args, gazer_argv)

        gazer_argv.append(bc_file)

        gazer_success = subprocess.run(gazer_argv)
        if gazer_success.returncode != 0:
            error_msg("gazer-theta exited with a failure.")
            return (1, {})

        theta_argv = [
            "java", "-jar", theta_cfa_path,
            "--model", theta_input
        ]

        args_dict = vars(args)
        for option in ["domain", "encoding", "initprec", "precgranularity", "predsplit", "refinement", "search", "maxenum"]:
            theta_argv.extend(["--{0}".format(option), args_dict[option]])

        if args.trace:
            theta_argv.append("--cex")

        theta_success = subprocess.run(theta_argv, env={'LD_LIBRARY_PATH': "$LD_LIBRARY_PATH:{0}".format(args.z3_path)})
        if theta_success.returncode != 0:
            error_msg("theta exited with a failure.")
            return (1, {})

        return (0, {})


def print_help(commands):
    print("usage: gazer <command> [<args>]\n")
    print("available commands are:")
    for name, cmd in commands.items():
        print("    {0:<15}    {1}".format(name, cmd.help))


def run_commands():
    # Register all commands
    commands = {
        'clang': ClangCommand(),
        'bmc': GazerBmcCommand(),
        'print-cfa': PrintCfaCommand(),
        'theta': ThetaCommand()
    }

    if len(sys.argv) < 2:
        print_help(commands)
        return 1

    cmdname = sys.argv[1]
    if cmdname not in commands:
        print("Unknown command `{0}`.".format(cmdname))
        print_help(commands)
        
    current = commands[cmdname]

    parser = argparse.ArgumentParser(prog="gazer {0}".format(cmdname))
    current.configure_args(parser)

    args = parser.parse_args(sys.argv[2:])

    # Create a temporary working directory
    wd = create_temp_dir()
    #print("Working directory is {0}".format(wd))

    current.run(args, wd)