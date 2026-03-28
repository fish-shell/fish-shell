#RUN: %fish %s
#REQUIRES: command -v python3

# Regression test for groff \X'...' device control escapes in man pages.
# help2man 1.50+ emits \X'tty: link URL' hyperlinks which broke the parser.
# See: coreutils 9.10 man pages.

set -l script (status dirname)/../../share/tools/create_manpage_completions.py
set -l tmpdir (mktemp -d)

# Minimal man page with \X'tty: link' escapes as produced by help2man 1.50
printf '%s\n' \
    '.TH TESTCMD "1" "March 2026" "test 1.0" "User Commands"' \
    '.SH NAME' \
    'testcmd \\- test command' \
    '.SH DESCRIPTION' \
    'A test command.' \
    '.TP' \
    '\\X'"'"'tty: link https://example.com/a'"'"'\\fB\\-a, \\-\\-all\\fP\\X'"'"'tty: link'"'"'' \
    'show all entries' \
    '.TP' \
    '\\X'"'"'tty: link https://example.com/v'"'"'\\fB\\-v, \\-\\-verbose\\fP\\X'"'"'tty: link'"'"'' \
    'be verbose' \
    '.TP' \
    '\\X'"'"'tty: link https://example.com/h'"'"'\\fB\\-\\-help\\fP\\X'"'"'tty: link'"'"'' \
    'display help' \
    '.PP' \
    'Some trailing paragraph text.' \
    '.SH AUTHOR' \
    'Nobody.' \
    > $tmpdir/testcmd.1

python3 $script --stdout $tmpdir/testcmd.1 | string match -r '^complete.*'
#CHECK: complete -c testcmd -s a -l all -d 'show all entries'
#CHECK: complete -c testcmd -s v -l verbose -d 'be verbose'
#CHECK: complete -c testcmd -l help -d 'display help'

rm -rf $tmpdir
