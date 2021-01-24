package Options;
use strict;
use warnings;

use Logger;

use Getopt::Long;
use File::Basename;
use POSIX qw(strftime);
use Cwd;
use Cwd 'abs_path';

sub initialize_flags {
    GetOptions( # the values of most options are globally accessible with the Options namespace
        'version'            => \my $version,
        'configuration=s'    => \$Options::config_path_name,    # relative to wd
        'task=s'             => \$Options::task_path_name,      # relative to wd
        'output-directory=s' => \$Options::output_directory,    # relative to wd
                                                                # log-levels:
         # - minimal: lists the commands used, flow of executing the steps, results of configs, and the final result - nothing else,
         # - (default) verbose: minimal + tool-output,
        'log-level=s' => \$Options::log_level,

# if used, the script outputs more data about the workings of the script itself, e.g. directories used, config list, etc. for debugging purposes (but independent of log-level)
        'debug' => \$Options::debug
    ) or die "Invalid options passed to $0\n";

    if ( not defined $version ) {
        lint_flags()
          ; # check, if all required flags are present and the values of them are valid
        my $wd = getcwd() . "/";    # get the working dir
        $Options::task_path_name =
          abs_path($Options::task_path_name);    # use the absolute path
        $Options::task_file_name = "/"
          . basename($Options::task_path_name)
          ;    # we'll need the file name from the path as well

        # initialize output directory
        my $iso8601_timestamp = strftime "%Y-%m-%dT%H:%M:%S", gmtime;
        if ( not defined $Options::output_directory )
        { # if the output directory wasn't explicitly set, then we put it in wd/portfolio-output-timestamp/
            $Options::output_directory =
              "${wd}portfolio-output-$iso8601_timestamp";
        }
        else
        { # if an output directory was given, than we create the portfolio-output-timestamp dir there
            $Options::output_directory =
              abs_path($Options::output_directory); # chop of /, if there is one
            $Options::output_directory = $Options::output_directory
              . "/portfolio-output-$iso8601_timestamp";
        }
        `mkdir -p $Options::output_directory`
          unless ( -d $Options::output_directory )
          ;    # create output dir, if we need to

# Logger only works after calling this subroutine, but puts logfile into output dir, so that part should be done already
        Logger::initialize_logfile($iso8601_timestamp);
    }
    else {     # --version received
        print get_version()
          ; # not logged but printed, as log is not initialized if --version was given - we don't really need to log this into a file
        exit;
    }
}

sub initialize_gazer_path {

    # script_path: path to the scripts folder in the gazer project
    my ($script_path) = @_;
    my $gazer_path = abs_path( dirname( dirname( abs_path($script_path) ) ) );
    my $build_path = $gazer_path . "/build/tools";
    my $tools_path = $gazer_path . "/tools";
    if ( -d $build_path ) {
        $Options::gazer_path = $build_path;
    }
    elsif ( -d $tools_path ) {
        $Options::gazer_path = $tools_path;
    }
    else {
        Logger::log_minimal(
"ERROR: Gazer tool directory gazer/build/tools or gazer/tools does not exist! Terminating..."
        );
        terminate();

    }
}

sub get_version {    # needs gazer_path to be initialized at this point
    my $z3path = "$Options::gazer_path/gazer-theta/theta/lib";
    $ENV{"LD_LIBRARY_PATH"} = $z3path;
    my $theta_version =
`java -Djava.library.path=$z3path -jar $Options::gazer_path/gazer-theta/theta/theta-cfa-cli.jar --version 2>&1`;

    my $gazer_version =
      `$Options::gazer_path/gazer-bmc/gazer-bmc --version 2>&1`
      ;    # tool hardcoded, as all gazer tools output the same version
           # print $gazer_version;
    return "$gazer_version\nTheta v$theta_version";
}

sub lint_flags {

    # check if required flags are present
    if ( not defined $Options::config_path_name ) {
        die
"ERROR: YAML configuration file missing (use the flag --configuration/-c).\nTerminating...";
    }
    elsif ( not defined $Options::task_path_name ) {
        die
          "ERROR: input file missing (use the flag --task/-t).\nTerminating...";
    }

    # check, if --log-level is present and initialize log level
    if ( not defined $Options::log_level ) {    # the default is verbose
        $Options::log_level = "verbose";
    }
    unless ( $Options::log_level eq "verbose"
        or $Options::log_level eq "minimal" )
    {
        die
"ERROR: Invalid log-level value (valid: minimal, verbose)\nTerminating...";
    }

# check if -d/--debug was given (this outputs more information about the workings of the script - it is independent of the log-level used)
    if ( not defined $Options::debug ) {
        $Options::debug = 0;
    }

    # check, if input and yml files exist
    unless ( -f $Options::task_path_name ) {
        die "ERROR: Task file does not exist!\nTerminating...";
    }
    unless ( -f $Options::config_path_name ) {
        die "ERROR: Configuration file does not exist!\nTerminating...";
    }
}
1;
