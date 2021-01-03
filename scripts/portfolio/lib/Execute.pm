package Execute;
use strict;
use warnings;

use FindBin qw($Bin);
use lib "$Bin/lib";
use Options;
use Logger;

use IO::Select;
use Proc::Killfam;

sub run_command {

# arguments:
# command
# timeout
# return_0 - if this is 1, the subroutine returns ERROR if the return_code wasn't 0
# output format (optional)
    my ( $command, $timeout, $return_0, $output_format ) =
      @_;    # defining parameters
    my $result
      ; # for storing the "Verification failed/successful/etc. output or TIMEOUT/ERROR/NONE"

    eval {    # eval, so alarm can work as a timeout
              # handling of timeout
        my $pid;
        my $fhandle;
        local $SIG{TERM} = 'IGNORE'
          ; # the script itself is part of the process group SIGTERM-d in case of a timeout
        local $SIG{ALRM} = sub {
            $result = "TIMEOUT";
            killfam( 'TERM', $pid ) unless ( not defined $pid );
            close $fhandle          unless ( not defined $fhandle );
            die "\`$command\` timed out after ${timeout}sec";
        };
        $pid = open( $fhandle, '-|', $command )
          or die $!;    # Open a pipe to read output from a command.

        alarm $timeout
          ; # the timeout is evoked by alarm, which means, that we are counting walltime
            # Note: if timeout value is 0 then there is no timeout set

        while ( my $line = <$fhandle> ) {
            chomp($line);
            Logger::log_verbose($line);
            if ( defined $output_format and $line =~ /$output_format/ ) {
                $result = $line;
            }
        }
        Logger::log_verbose "";

        alarm 0;    # timeout wasn't reached, stop the timer
        close $fhandle;
    };

    unless ( $return_0 eq 0 or $? eq 0 or $result = "TIMEOUT" ) {
        $result = "ERROR";
    }
    if ( not defined $result ) {
        $result = "NONE";
    }

    # returns the output or TIMEOUT or ERROR
    return $result;
}

sub run_gazer {

    # arguments:
    # tool: gazer-bmc or gazer-theta
    # flags: string of flags
    # timeout
    my ( $config_name, $tool, $flags, $timeout ) = @_;    # defining parameters

    my $command_string =
        $Options::gazer_path . "/"
      . $tool . "/"
      . $tool . " "
      . $Options::task_path_name . " "
      . $flags
      . " 2>&1";    # concatenating command
    Logger::log_minimal "Running \`" . $command_string . "\`";

    return run_command( $command_string, $timeout, 1, "Verification.*" );
}

sub check_harness {

# arguments:
# harness compile
# execution timeout
# gazer output (the line, where the type of the problem is defined - div by zero, etc.)
    my ( $compile_timeout, $test_timeout, $gazer_output ) =
      @_;    # defining arguments

    my $clang_command =
"clang $Options::task_path_name $Options::output_directory$Options::task_file_name.ll -o $Options::output_directory${Options::task_file_name}_test 2>&1"
      ;      # concatenating command
    Logger::log_minimal "Running \`" . $clang_command . "\`";
    my $clang_result = run_command( $clang_command, $compile_timeout, 0 );

    return "Test harness compilation " . $clang_result
      unless ( $clang_result eq "NONE" )
      ;    # if the compilation wasn't successful, return with the TIMEOUT/ERROR

# reason for bash -c: if there is a signal without a handler (like an FPE),
# then the "Floating point exception(core dumped)" will be the output of bash, not the C program
    my $test_command =
"bash -c \"${Options::output_directory}${Options::task_file_name}_test; true\" 2>&1";

    # run test harness
    Logger::log_minimal "Running \`" . $test_command . "\`";
    my $required_test_output =
      "(Assertion(.)*failed)|(Floating point exception)";
    my $test_result =
      run_command( $test_command, $test_timeout, 0, $required_test_output );
    if ( $test_result =~ $required_test_output ) {
        return "Test harness SUCCESSFUL WITNESS";
    }
    else {
        return "Test harness WITNESS FAILED: " . $test_result;
    }
}
1;
