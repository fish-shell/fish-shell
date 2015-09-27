#!/bin/bash
# Expects to be called from Xcode (Run Script build phase),
# write version number C preprocessor macro to header file.

tmp="$SCRIPT_OUTPUT_FILE_1"
ver="$SCRIPT_OUTPUT_FILE_0"

./build_tools/git_version_gen.sh

cat FISH-BUILD-VERSION-FILE | awk '{printf("#define %s \"%s\"\n",$1,$3)}' > "$tmp"

cmp --quiet "$tmp" "$ver"
if [ $? -ne 0 ]; then
    /bin/mv "$tmp" "$ver"
else
    /bin/rm "$tmp"
fi
