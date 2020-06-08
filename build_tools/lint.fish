#!/usr/bin/env fish
#
# This is meant to be run by "make lint" or "make lint-all". It is not meant to
# be run directly from a shell prompt.
#

# We don't include "missingInclude" as that doesn't find our config.h.
# Missing includes will quickly be found by... compiling the thing anyway.
set -l cppchecks warning,performance,portability,information #,missingInclude
set -l cppcheck_args
set -l c_files
set -l all no
set -l kernel_name (uname -s)
set -l machine_type (uname -m)

argparse a/all p/project= -- $argv

# We only want -D and -I options to be passed thru to cppcheck.
for arg in $argv
    if string match -q -- '-D*' $arg
        set cppcheck_args $cppcheck_args $arg
    else if string match -q -- '-I*' $arg
        set cppcheck_args $cppcheck_args $arg
    else if string match -q -- '-iquote*' $arg
        set cppcheck_args $cppcheck_args $arg
    end
end

# Not sure when this became necessary but without these flags cppcheck no longer works on macOS.
# It complains that "Cppcheck cannot find all the include files." Adding these include paths should
# be harmless everywhere else.
set cppcheck_args $cppcheck_args -I /usr/include -I .

if test "$machine_type" = x86_64
    set cppcheck_args -D__x86_64__ -D__LP64__ $cppcheck_args
end

if set -q _flag_all
    set c_files src/*.cpp
    set cppchecks "$cppchecks,unusedFunction"
else
    # We haven't been asked to lint all the source. If there are uncommitted
    # changes lint those, else lint the files in the most recent commit.
    # Select (cached files) (modified but not cached, and untracked files)
    set -l files (git diff-index --cached HEAD --name-only)
    set files $files (git ls-files --exclude-standard --others --modified)
    if not set -q files[1]
        # No pending changes so lint the files in the most recent commit.
        set files (git diff-tree --no-commit-id --name-only -r HEAD)
    end

    # Extract just the C/C++ files that exist.
    set c_files
    for file in (string match -r '.*\.c(?:pp)?$' -- $files)
        test -f $file; and set c_files $c_files $file
    end
end

# We now have a list of files to check so run the linters.
if set -q c_files[1]
    if type -q include-what-you-use
        echo
        echo ========================================
        echo Running IWYU
        echo ========================================
        for c_file in $c_files
            switch $kernel_name
                case Darwin FreeBSD
                    include-what-you-use -Xiwyu --no_default_mappings -Xiwyu \
                        --mapping_file=build_tools/iwyu.osx.imp --std=c++11 \
                        $cppcheck_args $c_file 2>&1
                case Linux
                    include-what-you-use -Xiwyu --mapping_file=build_tools/iwyu.linux.imp \
                        $cppcheck_args $c_file 2>&1
                case '*' # hope for the best
                    include-what-you-use --std=c++11 $cppcheck_args $c_file 2>&1
            end
        end
    end

    if type -q cppcheck
        echo
        echo ========================================
        echo Running cppcheck
        echo ========================================
        # The stderr to stdout redirection is because cppcheck, incorrectly IMHO, writes its
        # diagnostic messages to stderr. Anyone running this who wants to capture its output will
        # expect those messages to be written to stdout.
        set -l cn (set_color normal)
        set -l cb (set_color --bold)
        set -l cu (set_color --underline)
        set -l cm (set_color magenta)
        set -l cbrm (set_color brmagenta)
        set -l template "[$cb$cu{file}$cn$cb:{line}$cn] $cbrm{severity}$cm ({id}):$cn\n {message}"
        set cppcheck_args -q --verbose --std=c++11 --std=posix --language=c++ --template $template \
            --suppress=missingIncludeSystem --inline-suppr --enable=$cppchecks \
            --rule-file=.cppcheck.rules --suppressions-list=.cppcheck.suppressions $cppcheck_args

        cppcheck $cppcheck_args $c_files 2>&1

        echo
        echo ========================================
        echo 'Running `cppcheck --check-config` to identify missing includes and similar problems.'
        echo 'Ignore unmatchedSuppression warnings as they are probably false positives we'
        echo 'cannot suppress.'
        echo ========================================
        cppcheck $cppcheck_args --check-config $c_files 2>&1
    end

    if type -q oclint
        echo
        echo ========================================
        echo Running oclint
        echo ========================================
        # The stderr to stdout redirection is because oclint, incorrectly writes its final summary
        # counts of the errors detected to stderr. Anyone running this who wants to capture its
        # output will expect those messages to be written to stdout.
        oclint $c_files -- $argv 2>&1
    end

    if type -q clang-tidy; and set -q _flag_project
        echo
        echo ========================================
        echo Running clang-tidy
        echo ========================================
        clang-tidy -p $_flag_project $c_files
    end
else
    echo
    echo 'WARNING: No C/C++ files to check'
    echo
end
