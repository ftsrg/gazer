package Logger;
use strict;
use warnings;

my $log;

sub initialize_logfile {
    my ($timestamp) = @_;
    open( $log, ">>", "$Options::output_directory." / ".$timestamp.log" )
      or die "Can't open logfile: $!\nTerminating...";
}

sub close_logfile {
    close($log);
}

sub log_h1 {

    # string to log into file
    my ($string) = @_;
    print "\n-- " . $string . " --\n";
    print $log "\n-- " . $string . " --\n";
}

sub log_h2 {

    # string to log into file
    my ($string) = @_;
    print "\n" . $string . "\n";
    print $log "\n" . $string . "\n";
}

sub log_minimal {

    # string to log into file
    my ($string) = @_;
    print $string. "\n";
    print $log $string . "\n";
}

sub log_verbose {

    # string to log into file
    my ($string) = @_;
    if ( $Options::log_level eq "verbose" ) {
        print $string. "\n";
        print $log $string . "\n";
    }
}

sub log_debug {

    # string to log into file
    my ($string) = @_;
    if ( $Options::debug eq 1 ) {
        print $string. "\n";
        print $log $string . "\n";
    }
}
1;
