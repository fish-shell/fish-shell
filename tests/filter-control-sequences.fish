# Remove the sorts of escape sequences interactive fish prints.

# First the enable sequences, then a "|" combiner and then the disable ones
set -l escapes "\e\[\?2004h"\
"\e\[>4;1m"\
"\e\[>5u"\
"\e="\
"|"\
"\e\[\?2004l"\
"\e\[>4;0m"\
"\e\[<1u"\
"\e>"

cat | string replace -ra -- $escapes ''
