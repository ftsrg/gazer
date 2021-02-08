# Gazer-Theta Portfolio

## What is the Portfolio script?

Gazer, and its backends can be used as a command-line tools (gazer-cfa, gazer-bmc or gazer-theta) with lots of possible configurations.
In those cases, Gazer simply executes the given configuration on the given input program and returns the result.
However, formal verification of software is a hard problem, so there is usually no single "best" configuration that is effective on all input programs.

The portfolio script adds another layer to the command-line tools by enabling the sequential execution of different configurations, which can be described in a YAML configuration file.
This way, multiple configurations can be tried for the input program, which together have a higher chance of succeeding than a single configuration alone.
Furthermore, portfolios that seem to be efficient can be saved in files to be reused later.

## Setting up the portfolio script

The portfolio script requires Perl with the following packages:
- `YAML::Tiny` *(Parses YAML, not part of core packages)*
- `Getopt::Long` *(Handles flags, part of core packages)*
- `Digest::file` *(generates SHA256 hash for witnesses, part of core packages)*
- `Process::Killfam` *(Required for properly terminating tools after timeout, not a core package)*

### Setting up via apt on Ubuntu

- https://packages.ubuntu.com/bionic/perl
- https://packages.ubuntu.com/bionic/libyaml-tiny-perl
- https://packages.ubuntu.com/bionic/libproc-processtable-perl

### Setting up through CPAN

```
perl -MCPAN -e shell
install <packages>
exit
```

## Using the portfolio script

The entry point of the portfolio script is `scripts/portfolio/Portfolio.pl`.

### Configuration file (YAML)

The configurations can be described using a YAML configuration file, which has to be passed to the portfolio script as `--configuration config.yml` or `-c config.yml`, where `config.yml` is an example YAML file.
The YAML configuration file has two main parts: the **global options** and the **description of the configurations**.

**Global options**

`generate-witness`: `Yes`/`No`

Gazer - among other formats - can generate a counterexample in the [format of SV-COMP's violation witnesses](https://github.com/sosy-lab/sv-witnesses).

`finish-all-configurations`: `Yes`/`No`

If `No`, the script stops as soon as the first configuration gives a conclusive (safe/unsafe) result, and returns this result.
If `Yes`, the script will run all the configurations in the order they were given in the configuration file.

`check-harness`

When defined Gazer generates an executable test harness if the result is unsafe.
The script compiles, executes and checks this test harness.
If the test harness does not trigger a property violation, the unsafe result is treated as unknown/inconclusive instead.
This can filter out false positives.
This option has two parameters: `compile-harness-timeout` and `execute-harness-timeout` describe the time limit for compiling and executing the harness (in seconds).
For example:
```
check-harness:
    execute-harness-timeout: 100
    compile-harness-timeout: 150
```

**Description of the configurations**

Configurations can be given as a list, where each item in the list corresponds to a particular configuration.
Their order in the list also defines their execution order.
For example:
```
configurations:
    - name: config1
      tool: gazer-bmc
      timeout: 1 # sec
      flags: --inline all --bound 1000000*
    - name: config2
      tool: gazer-theta
      flags: --inline all --domain EXPL
```
- The `name` and `tool` attributes are required, while `timeout` and `flags` are optional.
- The `name` can be an arbitrary identifier, which just makes the output easier to interpret. 
- The possible values of `tool` are currently `gazer-bmc` or `gazer-theta`. 
- Giving a `timeout` is optional, however it is recommended to avoid configurations getting stuck (e.g., an infinite loop in the verifier backend).
- The `flags` describe the additional flags given to the particular backend. See the documentation of [Gazer](https://github.com/ftsrg/gazer/blob/master/README.md) and [Theta](https://github.com/ftsrg/theta/blob/master/doc/CEGAR-algorithms.md) for the available options.

For more detailed examples, see `scripts/portfolio/preconfigured-portfolios`.

### Flags

The portfolio script has the following flags.

- `--version`/`-v`: outputs the Gazer and Theta versions *(and does nothing else)*.
- `--configuration`/`-c` `<FILENAME>`: configuration file in YAML format.
- `--task`/`-t` `<FILENAME>`: The input C program to be verified (`*.c`, `*.i`, `*.ll`).
- `--log-level`/`-l`: Level of logging, possible values are `minimal` or `verbose` (`verbose` outputs the output of Gazer as well).
- `--output-directory`/`-o` `<PATH>`: Output path, where a portfolio-output directory will be created and used by the script (default: working directory).
- `--debug`/`-d`: If given, the logs are extended with more information about the script for debugging purposes (directories used, configuration list, etc.).

### Example
```
./Portfolio.pl -c preconfigured-portfolios/config-file-example.yml -t test-files/test_locks_14-2.c -l minimal -o test_output/
```
