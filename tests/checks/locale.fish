# RUN: fish=%fish %fish %s
# This hangs when running on github actions with tsan for unknown reasons,
# see #7934.
#REQUIRES: test -z "$GITHUB_WORKFLOW"

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

# Since the previous change was localized to a block it should no
# longer be in effect and we should be back to a UTF-8 locale.
echo -n C\u00FCC | display_bytes
#CHECK: 0000000 103 303 274 103
#CHECK: 0000004

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
