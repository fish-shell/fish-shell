#!/usr/bin/env fish
#
# This is meant to be run by "make lint" or "make lint-all". It is not meant to
# be run directly from a shell prompt.
#
set cppchecks warning,performance,portability,information,missingInclude
set cppcheck_args
set c_files
set all no

set -gx CXX $argv[1]
set -e argv[1]

if test "$argv[1]" = "--all"
    set all yes
    set cppchecks "$cppchecks,unusedFunction"
    set -e argv[1]
end

# We only want -D and -I options to be passed thru to cppcheck.
for arg in $argv
    if string match -q -- '-D*' $arg
        set cppcheck_args $cppcheck_args $arg
    else if string match -q -- '-I*' $arg
        set cppcheck_args $cppcheck_args $arg
    end
end
if test (uname -m) = "x86_64"
    set cppcheck_args -D__x86_64__ -D__LP64__ $cppcheck_args
end

if test $all = yes
    set c_files src/*.cpp
else
    # We haven't been asked to lint all the source. If there are uncommitted
    # changes lint those, else lint the files in the most recent commit.
    set pending (git status --porcelain --short --untracked-files=all | sed -e 's/^ *//')
    if count $pending > /dev/null
        # There are pending changes so lint those files.
        for arg in $pending
            set files $files (string split -m 1 ' ' $arg)[2]
        end
    else
        # No pending changes so lint the files in the most recent commit.
        set files (git show --porcelain --name-only --pretty=oneline head | tail --lines=+2)
    end

    # Extract just the C/C++ files.
    set c_files (string match -r '.*\.c(?:pp)?$' -- $files)
end

# We now have a list of files to check so run the linters.
if set -q c_files[1]
    if type -q cppcheck
        echo
        echo ========================================
        echo Running cppcheck
        echo ========================================
        # The stderr to stdout redirection is because cppcheck, incorrectly
        # IMHO, writes its diagnostic messages to stderr. Anyone running
        # this who wants to capture its output will expect those messages to be
        # written to stdout.
        cppcheck -q --verbose --std=posix --std=c11 --language=c++ --template "[{file}:{line}]: {severity} ({id}): {message}" --suppress=missingIncludeSystem --inline-suppr --enable=$cppchecks $cppcheck_args $c_files 2>& 1
    end

    if type -q oclint
        echo
        echo ========================================
        echo Running oclint
        echo ========================================
        # The stderr to stdout redirection is because oclint, incorrectly
        # writes its final summary counts of the errors detected to stderr.
        # Anyone running this who wants to capture its output will expect those
        # messages to be written to stdout.
        if test (uname -s) = "Darwin"
            if not test -f compile_commands.json
                xcodebuild > xcodebuild.log
                oclint-xcodebuild xcodebuild.log > /dev/null
            end
            if test $all = yes
                oclint-json-compilation-database -e '/pcre2-10.20/' -- -enable-global-analysis 2>& 1
            else
                set i_files
                for f in $c_files
                    set i_files $i_files -i $f
                end
                echo oclint-json-compilation-database -e '/pcre2-10.20/' $i_files
                oclint-json-compilation-database -e '/pcre2-10.20/' $i_files 2>& 1
            end
        else
            # Presumably we're on Linux or other platform not requiring special
            # handling for oclint to work.
            oclint $c_files -- $argv 2>& 1
        end
    end
else
    echo
    echo 'WARNING: No C/C++ files to check'
    echo
end
