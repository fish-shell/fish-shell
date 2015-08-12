#! /bin/sh

# Script for testing regular expressions with perl to check that PCRE2 handles
# them the same. The Perl code has to have "use utf8" and "require Encode" at
# the start when running UTF-8 tests, but *not* for non-utf8 tests. (The
# "require" would actually be OK for non-utf8-tests, but is not always
# installed, so this way the script will always run for these tests.)
#
# The desired effect is achieved by making this a shell script that passes the
# Perl script to Perl through a pipe. If the first argument is "-utf8", a
# suitable prefix is set up.
#
# The remaining arguments, if any, are passed to Perl. They are an input file
# and an output file. If there is one argument, the output is written to
# STDOUT. If Perl receives no arguments, it opens /dev/tty as input, and writes
# output to STDOUT. (I haven't found a way of getting it to use STDIN, because
# of the contorted piping input.)

perl=perl
prefix=''
if [ $# -gt 0 -a "$1" = "-utf8" ] ; then
  prefix="use utf8; require Encode;"
  shift
fi


# The Perl script that follows has a similar specification to pcre2test, and so
# can be given identical input, except that input patterns can be followed only
# by Perl's lower case modifiers and certain other pcre2test modifiers that are
# either handled or ignored:
#
#   aftertext          interpreted as "print $' afterwards"
#   afteralltext       ignored
#   dupnames           ignored (Perl always allows)
#   mark               ignored
#   no_auto_possess    ignored
#   no_start_optimize  ignored
#   ucp                sets Perl's /u modifier
#   utf                invoke UTF-8 functionality
#
# The data lines must not have any pcre2test modifiers. They are processed as
# Perl double-quoted strings, so if they contain " $ or @ characters, these
# have to be escaped. For this reason, all such characters in the
# Perl-compatible testinput1 and testinput4 files are escaped so that they can
# be used for perltest as well as for pcre2test. The output from this script
# should be same as from pcre2test, apart from the initial identifying banner.
#
# The other testinput files are not suitable for feeding to perltest.sh,
# because they make use of the special modifiers that pcre2test uses for
# testing features of PCRE2. Some of these files also contain malformed regular
# expressions, in order to check that PCRE2 diagnoses them correctly.

(echo "$prefix" ; cat <<'PERLEND'

# Function for turning a string into a string of printing chars.

sub pchars {
my($t) = "";
if ($utf8)
  {
  @p = unpack('U*', $_[0]);
  foreach $c (@p)
    {
    if ($c >= 32 && $c < 127) { $t .= chr $c; }
      else { $t .= sprintf("\\x{%02x}", $c);
      }
    }
  }
else
  {
  foreach $c (split(//, $_[0]))
    {
    if (ord $c >= 32 && ord $c < 127) { $t .= $c; }
      else { $t .= sprintf("\\x%02x", ord $c); }
    }
  }
$t;
}


# Read lines from a named file or stdin and write to a named file or stdout;
# lines consist of a regular expression, in delimiters and optionally followed
# by options, followed by a set of test data, terminated by an empty line.

# Sort out the input and output files

if (@ARGV > 0)
  {
  open(INFILE, "<$ARGV[0]") || die "Failed to open $ARGV[0]\n";
  $infile = "INFILE";
  $interact = 0;
  }
else
  {
  open(INFILE, "</dev/tty") || die "Failed to open /dev/tty\n";
  $infile = "INFILE";
  $interact = 1;
  }

if (@ARGV > 1)
  {
  open(OUTFILE, ">$ARGV[1]") || die "Failed to open $ARGV[1]\n";
  $outfile = "OUTFILE";
  }
else { $outfile = "STDOUT"; }

printf($outfile "Perl $] Regular Expressions\n\n");

# Main loop

NEXT_RE:
for (;;)
  {
  printf "  re> " if $interact;
  last if ! ($_ = <$infile>);
  printf $outfile "$_" if ! $interact;
  next if ($_ =~ /^\s*$/ || $_ =~ /^#/);

  $pattern = $_;

  while ($pattern !~ /^\s*(.).*\1/s)
    {
    printf "    > " if $interact;
    last if ! ($_ = <$infile>);
    printf $outfile "$_" if ! $interact;
    $pattern .= $_;
    }

  chomp($pattern);
  $pattern =~ s/\s+$//;

  # Split the pattern from the modifiers and adjust them as necessary.

  $pattern =~ /^\s*((.).*\2)(.*)$/s;
  $pat = $1;
  $mod = $3;

  # The private "aftertext" modifier means "print $' afterwards".

  $showrest = ($mod =~ s/aftertext,?//);

  # "allaftertext" is used by pcre2test to print remainders after captures

  $mod =~ s/allaftertext,?//;

  # Detect utf

  $utf8 = $mod =~ s/utf,?//;

  # Remove "dupnames".

  $mod =~ s/dupnames,?//;

  # Remove "mark" (asks pcre2test to check MARK data) */

  $mod =~ s/mark,?//;

  # "ucp" asks pcre2test to set PCRE2_UCP; change this to /u for Perl

  $mod =~ s/ucp,?/u/;

  # Remove "no_auto_possess" and "no_start_optimize" (disable PCRE2 optimizations)

  $mod =~ s/no_auto_possess,?//;
  $mod =~ s/no_start_optimize,?//;

  # Add back retained modifiers and check that the pattern is valid.

  $mod =~ s/,//g;
  $pattern = "$pat$mod";
  eval "\$_ =~ ${pattern}";
  if ($@)
    {
    printf $outfile "Error: $@";
    if (! $interact)
      {
      for (;;)
        {
        last if ! ($_ = <$infile>);
        last if $_ =~ /^\s*$/;
        }
      }
    next NEXT_RE;
    }

  # If the /g modifier is present, we want to put a loop round the matching;
  # otherwise just a single "if".

  $cmd = ($pattern =~ /g[a-z]*$/)? "while" : "if";

  # If the pattern is actually the null string, Perl uses the most recently
  # executed (and successfully compiled) regex is used instead. This is a
  # nasty trap for the unwary! The PCRE2 test suite does contain null strings
  # in places - if they are allowed through here all sorts of weird and
  # unexpected effects happen. To avoid this, we replace such patterns with
  # a non-null pattern that has the same effect.

  $pattern = "/(?#)/$2" if ($pattern =~ /^(.)\1(.*)$/);

  # Read data lines and test them

  for (;;)
    {
    printf "data> " if $interact;
    last NEXT_RE if ! ($_ = <$infile>);
    chomp;
    printf $outfile "$_\n" if ! $interact;

    s/\s+$//;  # Remove trailing space
    s/^\s+//;  # Remove leading space

    last if ($_ eq "");
    $x = eval "\"$_\"";   # To get escapes processed

    # Empty array for holding results, ensure $REGERROR and $REGMARK are
    # unset, then do the matching.

    @subs = ();

    $pushes = "push \@subs,\$&;" .
         "push \@subs,\$1;" .
         "push \@subs,\$2;" .
         "push \@subs,\$3;" .
         "push \@subs,\$4;" .
         "push \@subs,\$5;" .
         "push \@subs,\$6;" .
         "push \@subs,\$7;" .
         "push \@subs,\$8;" .
         "push \@subs,\$9;" .
         "push \@subs,\$10;" .
         "push \@subs,\$11;" .
         "push \@subs,\$12;" .
         "push \@subs,\$13;" .
         "push \@subs,\$14;" .
         "push \@subs,\$15;" .
         "push \@subs,\$16;" .
         "push \@subs,\$'; }";

    undef $REGERROR;
    undef $REGMARK;

    eval "${cmd} (\$x =~ ${pattern}) {" . $pushes;

    if ($@)
      {
      printf $outfile "Error: $@\n";
      next NEXT_RE;
      }
    elsif (scalar(@subs) == 0)
      {
      printf $outfile "No match";
      if (defined $REGERROR && $REGERROR != 1)
        { printf $outfile (", mark = %s", &pchars($REGERROR)); }
      printf $outfile "\n";
      }
    else
      {
      while (scalar(@subs) != 0)
        {
        printf $outfile (" 0: %s\n", &pchars($subs[0]));
        printf $outfile (" 0+ %s\n", &pchars($subs[17])) if $showrest;
        $last_printed = 0;
        for ($i = 1; $i <= 16; $i++)
          {
          if (defined $subs[$i])
            {
            while ($last_printed++ < $i-1)
              { printf $outfile ("%2d: <unset>\n", $last_printed); }
            printf $outfile ("%2d: %s\n", $i, &pchars($subs[$i]));
            $last_printed = $i;
            }
          }
        splice(@subs, 0, 18);
        }

      # It seems that $REGMARK is not marked as UTF-8 even when use utf8 is
      # set and the input pattern was a UTF-8 string. We can, however, force
      # it to be so marked.

      if (defined $REGMARK && $REGMARK != 1)
        {
        $xx = $REGMARK;
        $xx = Encode::decode_utf8($xx) if $utf8;
        printf $outfile ("MK: %s\n", &pchars($xx));
        }
      }
    }
  }

# printf $outfile "\n";

PERLEND
) | $perl - $@

# End
