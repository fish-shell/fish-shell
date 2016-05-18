#!/usr/bin/env fish
#
# This is meant to be run by "make lint" or "make lint-all". It is not meant to
# be run directly from a shell prompt.
#
set cppchecks warning,performance,portability,information,missingInclude
set cppcheck_args
set c_files
set all no
set kernel_name (uname -s)
set machine_type (uname -m)

set -gx CXX $argv[1]
set -e argv[1]

if test "$argv[1]" = "--all"
    set all yes
    set cppchecks "$cppchecks,unusedFunction"
    set -e argv[1]
end

if test $kernel_name = Linux
    # This is an awful hack. However, the include-what-you-use program spews lots of errors like
    #   /usr/include/unistd.h:226:10: fatal error: 'stddef.h' file not found
    # if we don't explicitly tell it where to find the system headers on Linux. See
    # http://stackoverflow.com/questions/19642590/libtooling-cant-find-stddef-h-nor-other-headers/
    set -l sys_includes (eval $CXX -v -c src/builtin.cpp 2>&1 | \
        sed -n -e '/^#include <...> search/,/^End of search list/s/^ *//p')[2..-2]
    set -x CPLUS_INCLUDE_PATH (string join ':' $sys_includes)
end

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
if test "$machine_type" = "x86_64"
    set cppcheck_args -D__x86_64__ -D__LP64__ $cppcheck_args
end

if test $all = yes
    set c_files src/*.cpp
else
    # We haven't been asked to lint all the source. If there are uncommitted
    # changes lint those, else lint the files in the most recent commit.
    # Select (cached files) (modified but not cached, and untracked files)
    set files (git diff-index --cached HEAD --name-only) (git ls-files --exclude-standard --others --modified)
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
    if type -q iwyu
        # The stderr to stdout redirection is because cppcheck, incorrectly IMHO, writes its
        # diagnostic messages to stderr. Anyone running this who wants to capture its output will
        # expect those messages to be written to stdout.
        for c_file in $c_files
            echo
            echo ========================================
            echo Running IWYU on $c_file
            echo ========================================
            switch $kernel_name
                case Darwin
                    include-what-you-use -Xiwyu --no_default_mappings -Xiwyu --mapping_file=build_tools/iwyu.osx.imp $cppcheck_args $c_file 2>&1
                case Linux
                    include-what-you-use -Xiwyu --mapping_file=build_tools/iwyu.linux.imp $cppcheck_args $c_file 2>&1
                case '*' # hope for the best
                    include-what-you-use $cppcheck_args $c_file 2>&1
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
        cppcheck -q --verbose --std=posix --std=c11 --language=c++ --template "[{file}:{line}]: {severity} ({id}): {message}" --suppress=missingIncludeSystem --inline-suppr --enable=$cppchecks $cppcheck_args $c_files 2>&1
    end

    if type -q oclint
        echo
        echo ========================================
        echo Running oclint
        echo ========================================
        # The stderr to stdout redirection is because oclint, incorrectly writes its final summary
        # counts of the errors detected to stderr. Anyone running this who wants to capture its
        # output will expect those messages to be written to stdout.
        if test "$kernel_name" = "Darwin"
            if not test -f compile_commands.json
                xcodebuild >xcodebuild.log
                oclint-xcodebuild xcodebuild.log >/dev/null
            end
            if test $all = yes
                oclint-json-compilation-database -e '/pcre2-10.21/' -- -enable-global-analysis 2>&1
            else
                set i_files
                for f in $c_files
                    set i_files $i_files -i $f
                end
                echo oclint-json-compilation-database -e '/pcre2-10.21/' $i_files
                oclint-json-compilation-database -e '/pcre2-10.21/' $i_files 2>&1
            end
        else
            # Presumably we're on Linux or other platform not requiring special
            # handling for oclint to work.
            oclint $c_files -- $argv 2>&1
        end
    end
else
    echo
    echo 'WARNING: No C/C++ files to check'
    echo
end
