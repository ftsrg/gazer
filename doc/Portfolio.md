# Gazer-Theta Portfolio
## What is the Portfolio script?
Gazer can be used as a command-line tool (gazer-cfa, gazer-bmc or gazer-theta) with lots of possible configurations. But in those cases Gazer simply executes that configuration on the given input program and returns the output of the verification process. The portfolio script adds another "level" to these command-line tools: it enables the sequential run of several configurations based on a YAML configuration file. This, on one hand, enables the user to easily compare configurations, as some may be better suited for a specific set of inputs than others. On the other hand it also enables of saving such successful configuration groups - portfolios - which then can be reused later.

## Usage of script
### Configuration file (YAML)
This file configures the portfolio and it is given to the Portfolio script as the argument of the flag *--configuration/-c*
#### Global options
##### generate-witness: Yes/No
Gazer - among other formats - can generate a counterexample in the format of SV-Comps violation witnesses. More about the witnesses [here](https://github.com/sosy-lab/sv-witnesses).
##### finish-all-configurations: Yes/No
If Yes, the script will run all the configurations in the order they were given in the configuration file. 
If No, the script will stop, when a configuration (with test harness checked, if set so) gives back a result (failed or succesful) and will output that as the final result
##### check-harness
If defined, then Gazer will generate a test harness in the appropriate cases and the script will compile, execute and check this test harness, with which it can filter out false positive results execute-harness-timeout and compile-harness-timeout need to be given in this case as well (in seconds), for example:
```
check-harness:
    execute-harness-timeout: 100
    check-harness-timeout: 150
```
#### Configuration of the portfolio itself
An array of the configurations, something like this:
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
- The **name** and **tool** attributes are required, **timeout** and **flags** are optional.
- The **name** can be any arbitrary name, it mostly serves readability purposes. 
- The possible values of **tool** are *gazer-bmc* or *gazer-theta*. 
- Giving a **timeout** is optional, but not doing so can possibly result in an endless run, if Gazer or Theta gets stuck.
- Documentation of the available **flags**: [for Gazer](https://github.com/ftsrg/gazer/blob/master/README.md), [for Theta](https://github.com/ftsrg/theta/blob/master/doc/CEGAR-algorithms.md)

For more detailed examples, see the *test-configs* directory under *scripts/portfolio/*

### Flags
**--version/-v** - outputs the Gazer and Theta versions *(and does nothing else)*
**--configuration/-c "filename"** - YAML portfolio configuration file
**--task/-t "filename"** - C program to be verified (.c, .i, .ll)
**--log-level/-l** - minimal or verbose (verbose outputs the output of Gazer as well)
**--output-directory/-o** - output path, where a portfolio-output directory will be created and used by the script (default: working directory)
**--debug/-d**** - if given, the logs are extended with more information about the script for debugging purposes (directories used, configuration list, etc.)

### Example
./Portfolio.pl -c test-files/config-file-example.yml -t test-files/test_locks_14-2.c -l minimal -o test_output/ 

## Required Perl packages
YAML::Tiny *(Parses YAML, not part of core packages)*
Getopt::Long *(Handles flags, part of core packages)*
Digest::file *(generates SHA256 hash for witnesses, part of core packages)*
Process::Killfam *(Required for properly terminating tools after timeout, not a core package)*

### from apt on Ubuntu:
https://packages.ubuntu.com/bionic/perl
https://packages.ubuntu.com/bionic/libyaml-tiny-perl
https://packages.ubuntu.com/bionic/libproc-processtable-perl

### through CPAN:
```
perl -MCPAN -e shell
install <packages>
exit
```
