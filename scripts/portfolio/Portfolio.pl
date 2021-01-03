#!/usr/bin/perl
use strict;
use warnings;

use FindBin qw($Bin);
use lib "$Bin/lib";

use Execute qw ( run_gazer check_harness );
use Logger;
use Options qw ( initialize_flags initialize_gazer_path );    # handles inits

use YAML::Tiny 'LoadFile';
use File::Basename;
use Digest::file qw(digest_file_hex);

initialize_environment();

# Initializing YAML
# Open the config - [0] means the first document (the config should only have one)
my $config                 = YAML::Tiny->read($Options::config_path_name)->[0];
my @configurations_arr_ref = @{ $config->{configurations} };  # YAML config file

# Listing and initializing configurations from YAML
initialize_configs();

Logger::log_h1("2. Executing Portfolio");
my %results;
my $last_config;
my $successful = 0
  ; # flag, 1 if any configuration (optionally with a test harness check) resulted in success (positive/negative, but not timeout or error)

execute_configurations();

Logger::log_h1("3. Evaluating Results");
list_results();

unless ( $config->{"finish-all-configurations"} eq "Yes" )
{    # we only draw a conclusion, if finish all configs is No/not set
    if ($successful) {
        Logger::log_h2(
            "Final result of portfolio: " . $results{ $last_config->{name} } );
    }
    else {
        Logger::log_h2("Final result of portfolio: NONE");
    }
}

Logger::close_logfile;

### Subroutines of main part ###

sub initialize_environment {

    # Initalizing gazer path, so --version works, when initializing flags
    my $dirname = dirname(__FILE__);
    Options::initialize_gazer_path($dirname);

# Initialization of input flags, input files, output directory and logfile (which means, logging only works after this subroutine)
    Options::initialize_flags();

    # Listing of paths
    Logger::log_h1 "1. Initializing Portfolio";

# Although --version also exists (it only prints the version and does nothing else), from a traceability point it is nice to always log the version
    Logger::log_minimal Options::get_version();
    my $pwd = `pwd`;
    chomp($pwd);
    Logger::log_debug "Working directory: $pwd";
    Logger::log_debug "Path to portfolio script: $dirname";
    Logger::log_debug "Path to the gazer executables: $Options::gazer_path";
    Logger::log_debug "Output path: " . $Options::output_directory;
}

sub initialize_configs {
    foreach (@configurations_arr_ref) {    # for every given config
        my $config_string = ""
          ; # in this we'll put together the attributes of the configuration for logging
        $config_string =
          $config_string . $_->{name} . ":\n\t";    # get name of config
        $config_string =
          $config_string . $_->{tool} . "; ";       # get gazer tool used
        if ( defined $_->{timeout} ) {              # see if it has a timeout
            $config_string = $config_string . $_->{timeout} . "; ";
        }
        else {
            $config_string = $config_string . "no timeout; ";
            $_->{timeout} = 0;
        } # if there is no timeout, set it to 0 (that we'll behave the right way)

        if ( not defined $_->{flags} ) {
            $_->{flags} = "";
        }    # see if we got any flags
        $_->{flags} = $_->{flags}
          . " --trace"; # put --trace in it - for now we always run with --trace
        if ( defined $config->{"generate-witness"} )
        {               # adding flags for generating witness, if needed
            my $hash = digest_file_hex( $Options::task_path_name, "SHA-256" );
            $_->{flags} = $_->{flags}
              . " --witness ${Options::output_directory}${Options::task_file_name}.witness.graphml --hash $hash";
        }
        if ( defined $config->{"check-harness"} )
        {               # adding flags for generating a test harness, if needed
            $_->{flags} = $_->{flags}
              . " -test-harness=${Options::output_directory}${Options::task_file_name}.ll";
        }
        $config_string = $config_string . $_->{flags};
        Logger::log_debug $config_string;
    }

    # Warnings and errors
    foreach (@configurations_arr_ref) {
        if ( not defined $_->{tool} ) {
            Logger::log_minimal(
"ERROR: In $_->{name}: every configuration needs the tool attribute to be set to gazer-bmc or gazer-theta.\nTerminating..."
            );
            terminate();
        }
        if ( not defined $_->{tool} ) {
            Logger::log_minimal(
"ERROR: In $_->{name}: every configuration needs the name attribute to be set.\nTerminating..."
            );
            terminate();
        }

        if ( not defined $_->{timeout} ) {
            Logger::log_minimal(
"WARNING: No timeout set for configuration $_->{name}, portfolio can end up running endlessly, if not terminate()d manually!"
            );
        }
    }
    if ( defined $config->{"check-harness"} ) {
        if (
            not defined $config->{"check-harness"}->{"execute-harness-timeout"}
          )
        {
            Logger::log_minimal(
"ERROR in YAML: If check-harness is defined, execute-harness-timeout must also be defined as its child.\nTerminating..."
            );
            terminate();
        }
        elsif (
            not defined $config->{"check-harness"}->{"compile-harness-timeout"}
          )
        {
            Logger::log_minimal(
"ERROR in YAML: If check-harness is defined, compile-harness-timeout must also be defined as its child.\nTerminating..."
            );
            terminate();
        }
    }
}

sub execute_configurations {
    foreach (@configurations_arr_ref)
    {    # Iterating through the given configurations
        $last_config = $_;
        Logger::log_h2 "Running $_->{name}...";
        $results{ $_->{name} } =
          Execute::run_gazer( $_->{name}, $_->{tool}, $_->{flags},
            $_->{timeout} );    # run gazer-theta
        Logger::log_minimal $results{ $_->{name} };

        if ( $results{ $_->{name} } =~ /Verification FAILED/ )
        {                       # we found a potential problem

            if ( defined $config->{"check-harness"} )
            { # let's check the test harness, if check-harness is set to Yes in YAML
                Logger::log_minimal("Checking test harness...");
                my $test_harness_result = Execute::check_harness(
                    $config->{"check-harness"}->{"compile-harness-timeout"},
                    $config->{"check-harness"}->{"execute-harness-timeout"}
                );
                $results{ $_->{name} . "-test-harness" } = $test_harness_result;
                Logger::log_minimal $results{ $_->{name} . "-test-harness" };

                if ( $test_harness_result =~ /Test harness SUCCESSFUL WITNESS/ )
                {
                    $successful = 1;
                    if ( not defined $config->{"finish-all-configurations"}
                        or $config->{"finish-all-configurations"} eq "No" )
                    {
   # let's stop the portfolio if finish all configs isn't set to yes in the YAML
                        last;
                    }
                }

            }

        }
        elsif ( $results{ $_->{name} } =~ /Verification SUCCESSFUL/ )
        {    # nothing else to do, we're done, we found no problems
            $successful = 1;
            if ( not defined $config->{"finish-all-configurations"}
                or $config->{"finish-all-configurations"} eq "No" )
            {
   # let's stop the portfolio if finish all configs isn't set to yes in the YAML
                last;
            }
        }
    }
}

sub list_results {
    foreach (@configurations_arr_ref) {
        if ( defined $results{ $_->{name} } ) {
            Logger::log_minimal("Result of $_->{name}: $results{$_->{name}}");
            if ( defined $results{ $_->{name} . "-test-harness" } ) {
                Logger::log_minimal( "Result of "
                      . $_->{name}
                      . "-test-harness" . ": "
                      . $results{ $_->{name} . "-test-harness" } );
            }
            Logger::log_minimal("");
        }
    }
}

sub terminate() {
    close_logfile();
    exit;
}
