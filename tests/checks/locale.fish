# RUN: fish=%fish %fish %s
# This hangs when running on github actions with tsan for unknown reasons,
# see #7934.
#REQUIRES: test -z "$GITHUB_WORKFLOW"

# We typically try to force a utf8-capable locale,
# this turns that off.
set -gx fish_allow_singlebyte_locale 1

# A function to display bytes, necessary because GNU and BSD implementations of `od` have different output.
# We used to use xxd, but it's not available everywhere. See #3797.
#
# We use the lowest common denominator format, `-b`, because it should work in all implementations.
# I wish we could use the `-t` flag but it isn't available in every OS we're likely to run on.
#
function display_bytes
    od -b | sed -e 's/  */ /g' -e 's/  *$//'
end

# Verify that our UTF-8 locale produces the expected output.
echo -n A\u00FCA | display_bytes
#CHECK: 0000000 101 303 274 101
#CHECK: 0000004

# Verify that exporting a change to the C locale produces the expected output.
# The output should include the literal byte \xFC rather than the UTF-8 sequence for \u00FC.
begin
    set -lx LC_ALL C
    echo -n B\u00FCB | display_bytes
end
#CHECK: 0000000 102 374 102
#CHECK: 0000003

# Since the previous change was localized to a block it should no
# longer be in effect and we should be back to a UTF-8 locale.
echo -n C\u00FCC | display_bytes
#CHECK: 0000000 103 303 274 103
#CHECK: 0000004

# Verify that setting a non-exported locale var doesn't affect the behavior.
# The output should include the UTF-8 sequence for \u00FC rather than that literal byte.
# Just like the previous test.
begin
    set -l LC_ALL C
    echo -n D\u00FCD | display_bytes
end
#CHECK: 0000000 104 303 274 104
#CHECK: 0000004

# Verify that fish can pass through non-ASCII characters in the C/POSIX
# locale. This is to prevent regression of
# https://github.com/fish-shell/fish-shell/issues/2802.
#
# These tests are needed because the relevant standards allow the functions
# mbrtowc() and wcrtomb() to treat bytes with the high bit set as either valid
# or invalid in the C/POSIX locales. GNU libc treats those bytes as invalid.
# Other libc implementations (e.g., BSD) treat them as valid. We want fish to
# always treat those bytes as valid.

# The fish in the middle of the pipeline should be receiving a UTF-8 encoded
# version of the unicode from the echo. It should pass those bytes thru
# literally since it is in the C locale. We verify this by first passing the
# echo output directly to the `xxd` program then via a fish instance. The
# output should be "58c3bb58" for the first statement and "58c3bc58" for the
# second.
echo -n X\u00FBX | display_bytes
echo X\u00FCX | env LC_ALL=C $fish -c 'read foo; echo -n $foo' | display_bytes
#CHECK: 0000000 130 303 273 130
#CHECK: 0000004
#CHECK: 0000000 130 303 274 130
#CHECK: 0000004

# The next tests deliberately spawn another fish instance to test inheritance of env vars.

# This test is subtle. Despite the presence of the \u00fc unicode char (a "u"
# with an umlaut) the fact the locale is C/POSIX will cause the \xfc byte to
# be emitted rather than the usual UTF-8 sequence \xc3\xbc. That's because the
# few single-byte unicode chars (that are not ASCII) are generally in the
# ISO 8859-x char sets which are encompassed by the C locale. The output should
# be "59fc59".
env LC_ALL=C $fish -c 'echo -n Y\u00FCY' | display_bytes
#CHECK: 0000000 131 374 131
#CHECK: 0000003

# The user can specify a wide unicode character (one requiring more than a
# single byte). In the C/POSIX locales we substitute a question-mark for the
# unencodable wide char. The output should be "543f54".
env LC_ALL=C $fish -c 'echo -n T\u01FDT' | display_bytes
#CHECK: 0000000 124 077 124
#CHECK: 0000003

string match ö \Xc3\Xb6
#CHECK: ö

math 5 \X2b 5
#CHECK: 10

math 7 \x2b 7
#CHECK: 14

echo \xc3\xb6
# CHECK: ö
echo \Xc3\Xb6
# CHECK: ö
