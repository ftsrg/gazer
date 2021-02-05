# Using Gazer and Theta with Benchexec
## What is Benchexec?
*"A Framework for Reliable Benchmarking and Resource Measurement"*, [see here](https://github.com/sosy-lab/benchexec).
It is used as the benchmarking tool of [SV-Comp](https://sv-comp.sosy-lab.org/) and makes it easy to get detailed resource information or run the tools on a big batch of tasks.

## How to use it to benchmark Gazer and Theta
Currently the official tool integration is for the *old* portfolio used on SV-Comp 2021, not the configurable, newer version. If you'd like to use the old one, see the paragraph about reproducing SV-Comp 2021 results below.

### Install Benchexec
If you want to use the current, configurable portfolio, just install Benchexec ([based on this doc](https://github.com/sosy-lab/benchexec/blob/master/doc/INSTALL.md)), find the `benchexec/tools` directory where you installed it and replace `gazer-theta.py` with [this](https://github.com/AdamZsofi/benchexec/blob/master/benchexec/tools/gazer-theta.py) version of the file. 

### Create a benchmark definition and prepare tasks
For the benchmark definitions see [this doc](https://github.com/sosy-lab/benchexec/blob/master/doc/benchexec.md#input-for-benchexec) and the basic example [here](https://github.com/ftsrg/gazer/tree/master/scripts/portfolio/basic_benchmark_definition.xml).

For tasks see [this doc](https://github.com/sosy-lab/benchexec/blob/master/doc/benchexec.md#defining-tasks-for-benchexec) and the [repository of tasks](https://github.com/sosy-lab/sv-benchmarks) used at SV-Comp.

### Run the benchmark
See [this doc](https://github.com/sosy-lab/benchexec/blob/master/doc/benchexec.md#starting-benchexec)

*Note: I recommend trying the portfolio without Benchexec first on the benchmarking machine - at least with --version - to see if every dependency is installed.*

## Note on the SV-Comp 2021 runs and reproducibility
On SV-Comp 2021 we used an earlier version of the portfolio (a non-configurable python script). The new, configurable one follows that one in principles, but it went through a major refactor and extension.

This new portfolio already changed a bit in regards to test harness checking, is under active development and will probably change more in the future as well.

### How to **use the same portfolio**, as on SV-Comp 2021
Use the configuration file [scripts/portfolio/preconfigured-portfolios/SVComp2021_configuration.yml](https://github.com/ftsrg/gazer/tree/master/scripts/portfolio/preconfigured-portfolios/SVComp2021_configuration.yml). It runs the *same configurations* of Gazer and Theta and uses a test harness check as well.

*NOTE: This test harness check works a bit differently, than the one used originally on the competition.*

On using the portfolio itself [see doc/Portfolio.md](https://github.com/ftsrg/gazer/blob/master/doc/Portfolio.md)
On using the portfolio with Benchexec, see above.

### How to **reproduce** the results of SV-Comp 2021
- Use [Theta v2.5.0](https://github.com/ftsrg/gazer/releases/tag/v1.2.1) and [Gazer v1.2.1](https://github.com/ftsrg/gazer/releases/tag/v1.2.1) *(A binary version with these versions for Ubuntu 18.04 and 20.04 is available [here](https://gitlab.com/sosy-lab/sv-comp/archives-2021/-/blob/master/2021/gazer-theta.zip))*
- Use the [benchmark definition](https://github.com/sosy-lab/sv-comp/blob/ea4cecb32463b94df7cf0c3b3b04ad5f671ee862/benchmark-defs/gazer-theta.xml) used at SV-Comp 2021
- Use [Benchexec v3.6](https://github.com/sosy-lab/benchexec/releases/tag/3.6) *(used at SV-Comp 2021)*
