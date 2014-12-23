# vim: set ts=4 sw=4 et:
# Utilities for the test runners

if test "$argv[1]" = (status -f)
    echo 'test_util.fish requires sourcing script as argument to `source`' >&2
    echo 'use `source test_util.fish (status -f); or exit`' >&2
    status --print-stack-trace >&2
    exit 1
end

# Any remaining arguments are passed back to test.fish
set -l args_for_test_script
if set -q argv[2]
    set args_for_test_script $argv[2..-1]
end

function die
    set -q argv[1]; and echo $argv[1] >&2
    exit 1
end

# check if we're running in the test environment
# if not, set it up and rerun fish with exec.
# The test is whether the special var __fish_is_running_tests
# exists and contains the same value as XDG_CONFIG_HOME. It checks
# the value and not just the presence because we're going to delete
# the config directory later if we're exiting successfully.
if not set -q __fish_is_running_tests
    # set up our test environment and re-run the original script
    set -l script $argv[1]
    switch $script
    case '/*'
        # path is absolute
    case '*'
        # path is relative, make it absolute
        set script $PWD/$script
    end
    set -l IFS # clear IFS so cmd substitution doesn't split
    cd (dirname $script); or die
    set -xl XDG_CONFIG_HOME (printf '%s/tmp.config/%s' $PWD %self); or die
    if test -d $XDG_CONFIG_HOME
        # if the dir already exists, we've reused a pid
        # this would be surprising to reuse a fish pid that failed in the past,
        # but clear it anyway
        rm -r $XDG_CONFIG_HOME; or die
    end
    mkdir -p $XDG_CONFIG_HOME/fish; or die
    ln -s $PWD/test_functions $XDG_CONFIG_HOME/fish/functions; or die
    set -l escaped_parent (dirname $PWD | sed -e 's/[\'\\\\]/\\\\&/g'); or die
    set -l escaped_config (printf '%s/fish' $XDG_CONFIG_HOME | sed -e 's/[\'\\\\]/\\\\&/g'); or die
    printf 'set fish_function_path \'%s/functions\' \'%s/share/functions\'\n' $escaped_config $escaped_parent > $XDG_CONFIG_HOME/fish/config.fish; or die
    set -xl __fish_is_running_tests $XDG_CONFIG_HOME
    # set locale information to be consistent
    set -lx LANG C
    set -lx LC_ALL ''
    for var in ALL COLLATE MESSAGES MONETARY NUMERIC TIME
        set -lx LC_$var ''
    end
    set -lx LC_CTYPE en_US.UTF-8
    exec ../fish $script $args_for_test_script
    die 'exec failed'
else if test "$__fish_is_running_tests" != "$XDG_CONFIG_HOME"
    echo 'Something went wrong with the test runner.' >&2
    echo "__fish_is_running_tests: $__fish_is_running_tests" >&2
    echo "XDG_CONFIG_HOME: $XDG_CONFIG_HOME" >&2
    exit 10
else
    # we're running tests with a temporary config directory
    function test_util_on_exit --on-process-exit %self -V __fish_is_running_tests
        if not set -q __fish_test_keep_tmp_config
            # remove the temporary config directory
            # unfortunately if this fails we can't alter the exit status of fish
            if not rm -r "$__fish_is_running_tests"
                echo "error: Couldn't remove temporary config directory '$__fish_is_running_tests'" >&2
            end
        end
    end
    # unset __fish_is_running_tests so any children that source
    # test_util.fish will set up a new test environment.
    set -e __fish_is_running_tests
end

set -l suppress_color
if not tty 0>&1 >/dev/null
    set suppress_color yes
end

function say -V suppress_color
    set -l color_flags
    set -l suppress_newline
    while set -q argv[1]
        switch $argv[1]
            case -b -o -u
                set color_flags $color_flags $argv[1]
            case -n
                set suppress_newline 1
            case --
                set -e argv[1]
                break
            case -\*
                continue
            case \*
                break
        end
        set -e argv[1]
    end

    if not set -q argv[2]
        echo 'usage: say [flags] color string [string...]' >&2
        return 1
    end

    if begin; test -n "$suppress_color"; or set_color $color_flags $argv[1]; end
        printf '%s' $argv[2..-1]
        test -z "$suppress_color"; and set_color reset
        if test -z "$suppress_newline"
            echo
        end
    end
end

function colordiff -d 'Colored diff output for unified diffs'
    diff $argv | while read -l line
        switch $line
            case '+*'
                say green $line
            case '-*'
                say red $line
            case '@*'
                say cyan $line
            case '*'
                echo $line
        end
    end
end
