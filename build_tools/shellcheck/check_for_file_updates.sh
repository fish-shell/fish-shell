#!/bin/sh

# Checks if the list of files to shellcheck is up-to-date.
# Exits 0 if it is, 1 otherwise.
# If the `--update` flag is provided, the list of files to check is updated instead of checked.
# Then, a non-zero exit code indicates some failure in the script.
#
# If you want to exclude paths from these checks, create a file called `excluded.txt` in this
# directory. For each file you want to exclude, put its relative path starting from the repo root
# into a line of the file.
# Make sure that the file does not have any other content or empty lines.
# Also ensure that `excluded.txt` is not empty.
# The file is intended for locally ignoring scripts not committed in the repo, which is why
# `excluded.txt` is already gitignored.
# If a file which is committed into the repo should not be checked, disable the checks via
# annotations in that file, either for each lint individually or by adding
# # shellcheck disable=all
# at the start of the file (but after the sheband).

set -e

cleanup () {
    # shellcheck disable=2317
    if [ -n "$tmp_file" ] && [ -e "$tmp_file" ]; then
        rm "$tmp_file"
    fi
}

trap cleanup EXIT INT TERM HUP

repo_root="$(dirname "$0")/../.."
cd "$repo_root"
shellcheck_dir="$PWD/build_tools/shellcheck"
included_file="$shellcheck_dir/included.txt"
excluded_file="$shellcheck_dir/excluded.txt"

while [ $# -gt 0 ]; do
    case $1 in
        --update)
            update_included_files=true
            ;;
        *)
            echo "Invalid argument: $1"
            exit 1
            ;;
    esac
    shift
done

tmp_file="$(mktemp)"

if [ -f "$excluded_file" ]; then
    filter_excluded='rg --invert-match --fixed-strings --file='"$excluded_file"
else
    filter_excluded='cat'
fi

# First, identify shell scripts by the presence of a shebang on the first line.
# Ripgrep respects .gitignore files, so files ignored there are automatically ignored here as well.
# By default, .gitignore files are only ignored within a git repo. In some situations, like in
# non-colocated jj repos, ripgrep cannot identify the repo root, resulting in .gitignore files not
# being taken into account.
# The `--no-require-git` flag prevents this, at the cost of also respecting .gitignore files outside
# of the repo on in ancestor directories.
#
# By default, ripgrep also considers rules in .git/info/exclude, which only exist locally.
# This script should behave the same way everywhere, so we do not want to take local configuration
# into account, which is why we disable this feature with `--no-ignore-exclude`.
#
# `--stop-on-nonmatch` ensures that we only consider the first line of each file. This improves
# performance and reduces the likelihood of false positives.
#
# `--files-with-matches` means that instead of the matched shebang line, the path of the file
# containing it will be printed.
#
# The pattern at the end is used to identify shebang lines. Importantly, fish shebangs should not be
# matched.
#
# The output of the first command will contain the paths of all files containing a shell shebang.
#
# The next command in the pipeline filters out any paths which are explicitly ignored in the
# corresponding file (build_tools/shellcheck/excluded.txt).
# `--fixed-strings` is used to prevent paths from being interpreted as regular expressions.
#
# Finally, the output is sorted, because ripgrep's output is unsorted.
# Setting the locale is important, because the behavior of sort is locale-dependent.
rg --no-require-git --no-ignore-exclude --stop-on-nonmatch --files-with-matches '^#!.*[^i]sh' \
    | $filter_excluded \
    | LC_ALL=C.UTF-8 sort > "$tmp_file"

if [ "$update_included_files" = true ]; then
    cp "$tmp_file" "$included_file"
else
    diff "$included_file" "$tmp_file"
fi
