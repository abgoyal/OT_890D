#!/usr/bin/perl -w
use strict;

my ($dir, $arch, @files) = @ARGV;

my $ret = 0;
my $line;
my $lineno = 0;
my $filename;

foreach my $file (@files) {
	local *FH;
	$filename = $file;
	open(FH, "<$filename") or die "$filename: $!\n";
	$lineno = 0;
	while ($line = <FH>) {
		$lineno++;
		&check_include();
		&check_asm_types();
		&check_sizetypes();
		&check_prototypes();
		&check_config();
	}
	close FH;
}
exit $ret;

sub check_include
{
	if ($line =~ m/^\s*#\s*include\s+<((asm|linux).*)>/) {
		my $inc = $1;
		my $found;
		$found = stat($dir . "/" . $inc);
		if (!$found) {
			$inc =~ s#asm/#asm-$arch/#;
			$found = stat($dir . "/" . $inc);
		}
		if (!$found) {
			printf STDERR "$filename:$lineno: included file '$inc' is not exported\n";
			$ret = 1;
		}
	}
}

sub check_prototypes
{
	if ($line =~ m/^\s*extern\b/) {
		printf STDERR "$filename:$lineno: extern's make no sense in userspace\n";
	}
}

sub check_config
{
	if ($line =~ m/[^a-zA-Z0-9_]+CONFIG_([a-zA-Z0-9]+)[^a-zA-Z0-9]/) {
		printf STDERR "$filename:$lineno: leaks CONFIG_$1 to userspace where it is not valid\n";
	}
}

my $linux_asm_types;
sub check_asm_types()
{
	if ($filename =~ /types.h|int-l64.h|int-ll64.h/o) {
		return;
	}
	if ($lineno == 1) {
		$linux_asm_types = 0;
	} elsif ($linux_asm_types >= 1) {
		return;
	}
	if ($line =~ m/^\s*#\s*include\s+<asm\/types.h>/) {
		$linux_asm_types = 1;
		printf STDERR "$filename:$lineno: " .
		"include of <linux/types.h> is preferred over <asm/types.h>\n"
		# Warn until headers are all fixed
		#$ret = 1;
	}
}

my $linux_types;
sub check_sizetypes
{
	if ($filename =~ /types.h|int-l64.h|int-ll64.h/o) {
		return;
	}
	if ($lineno == 1) {
		$linux_types = 0;
	} elsif ($linux_types >= 1) {
		return;
	}
	if ($line =~ m/^\s*#\s*include\s+<linux\/types.h>/) {
		$linux_types = 1;
		return;
	}
	if ($line =~ m/__[us](8|16|32|64)\b/) {
		printf STDERR "$filename:$lineno: " .
		              "found __[us]{8,16,32,64} type " .
		              "without #include <linux/types.h>\n";
		$linux_types = 2;
		# Warn until headers are all fixed
		#$ret = 1;
	}
}

