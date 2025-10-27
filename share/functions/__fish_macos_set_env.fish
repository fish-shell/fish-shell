# localization: tier1
# Adapt construct_path from the macOS /usr/libexec/path_helper
# executable for fish; see
# https://opensource.apple.com/source/shell_cmds/shell_cmds-203/path_helper/path_helper.c.auto.html .
function __fish_macos_set_env -d "set an environment variable like path_helper does (macOS only)"
    set -l result

    # Populate path according to config files
    for path_file in $argv[2] $argv[3]/*
        for entry in (string split : <? $path_file)
            if not contains -- $entry $result
                test -n "$entry"
                and set -a result $entry
            end
        end
    end

    # Merge in any existing path elements
    for existing_entry in $$argv[1]
        if test -n $existing_entry; and not contains -- $existing_entry $result
            set -a result $existing_entry
        end
    end
    if test $argv[1] = MANPATH
        set -a result ""
    end

    set -xg $argv[1] $result
end
