#!/bin/bash
# Expects to be called from Xcode (Run Script build phase),
# write version number C preprocessor macro to header file.

ver="$SCRIPT_OUTPUT_FILE_0"

./build_tools/git_version_gen.sh

cmp --quiet "FISH-BUILD-VERSION-FILE" "$ver"
if [ $? -ne 0 ]; then
    /bin/cp FISH-BUILD-VERSION-FILE "$ver"
fi
