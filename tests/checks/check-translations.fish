#RUN: %fish -C 'set -l fish %fish' %s
#REQUIRES: msgfmt --help

set -l fail_count 0
for file in $FISH_SOURCE_DIR/po/*.po
    # We only check the format strings.
    # Later on we might do a full "--check" to also check the headers.
    msgfmt --check-format $file
    or set fail_count (math $fail_count + 1)
end

# Prevent setting timestamp if any errors were encountered
if test "$fail_count" -gt 0
    exit 1
end

# No output is good output
