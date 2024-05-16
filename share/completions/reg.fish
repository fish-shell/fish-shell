function __reg_run_reg_safely
    set -l output (reg $argv | tr -d '\r' | tail --lines +2 | string collect)
    if not string match -q -r "reg: Invalid syntax*" -- $output
        set output (string split \n -- $output)
        echo $output | string replace -a " " \n
    end
end

function __reg_add_complete_args -a previous_token
    if test "$previous_token" = add
        set -l current_token (commandline -tc)
        __reg_run_reg_safely query $current_token
        return
    end

    if test "$previous_token" = /t
        echo 'REG_SZ
REG_MULTI_SZ
REG_DWORD_BIG_ENDIAN
REG_DWORD
REG_BINARY
REG_DWORD_LITTLE_ENDIAN
REG_LINK
REG_FULL_RESOURCE_DESCRIPTOR
REG_EXPAND_SZ'
        return
    end

    if not __fish_seen_argument -w v -w ve
        echo -e '/v\tSpecify the name of the add registry entry
/ve\tSpecify that the added registry entry has a null value'
    end

    echo -e '/t\tSpecify the type for the registry entry
/s\tSpecify the character to be used
/d\tSpecify the data for the new registry entry
/f\tAdd the registry entry without prompting for confirmation
/?\tShow help'
end

function __reg_compare_complete_args
    if not __fish_seen_argument -w v -w ve
        echo -e '/v\tSpecify the value name
/ve\tSpecify that only entries that have a value name of null should be compared'
    end

    if not __fish_seen_argument -w oa -w od -w os -w on
        echo -e '/oa\tSpecify that all differences and matches are displayed
/od\tSpecify that only differences are displayed
/os\tSpecify that only matches are displayed
/on\tSpecify that nothing is displayed'
    end

    echo -e '/s\tCompare all subkeys and entries recursively
/?\tShow help'
end

function __reg_copy_complete_args
    echo -e '/s\tCopy all subkeys and entries under the specified subkey
/f\tCopy the subkey without prompting for confirmation
/?\tShow help'
end

function __reg_delete_complete_args -a previous_token
    if test "$previous_token" = delete
        set -l current_token (commandline -tc)
        __reg_run_reg_safely query $current_token
        return
    end

    if not __fish_seen_argument -w v -w ve -w va
        echo -e '/v\tDelete a specific entry under the subkey
/ve\tSpecify that only entries that have no value will be deleted
/va\tDelete all entries under the specified subkey'
    end

    echo -e '/f\tDelete the existing registry subkey or entry without asking for confirmation
/?\tShow help'
end

function __reg_export_complete_args -a previous_token
    if test "$previous_token" = export
        set -l current_token (commandline -tc)
        __reg_run_reg_safely query $current_token
        return
    end

    echo -e '/y\tOverwrite any existing file with the name filename without prompting for confirmation
/?\tShow help'
end

function __reg_query_complete_args -a previous_token
    if test "$previous_token" = query
        set -l current_token (commandline -tc)
        __reg_run_reg_safely query $current_token
        return
    end

    if test "$previous_token" = /t
        echo 'REG_SZ
REG_MULTI_SZ
REG_EXPAND_SZ
REG_DWORD
REG_BINARY
REG_NONE'
        return
    end

    if not __fish_seen_argument -w v -w ve
        echo -e '/v\tSpecify the registry value name
/ve\tRun a query for value names that are empty'
    end

    if not __fish_seen_argument -w k -w d
        echo -e '/k\tSpecify to search in key names only
/d\tSpecify to search in data only'
    end

    echo -e '/se\tSpecify the single value separator
/f\tSpecify the data or pattern to search for
/c\tSpecify that the query is case sensitive
/e\tSpecify to return only exact matches
/t\tSpecify registry types to search
/z\tSpecify to include the numeric equivalent for the registry type in search results
/?\tShow help'
end

function __reg_save_complete_args -a previous_token
    if test "$previous_token" = save
        set -l current_token (commandline -tc)
        __reg_run_reg_safely query $current_token
        return
    end

    echo -e '/y\tOverwrite an existing file with the name filename without prompting for confirmation
/?\tShow help'
end

function __reg_complete_args -d 'Function to generate args'
    set -l previous_token (commandline -xc)[-1]

    if __fish_seen_subcommand_from add
        __reg_add_complete_args $previous_token
    else if __fish_seen_subcommand_from compare
        __reg_compare_complete_args
    else if __fish_seen_subcommand_from copy
        __reg_copy_complete_args
    else if __fish_seen_subcommand_from delete
        __reg_delete_complete_args $previous_token
    else if __fish_seen_subcommand_from export
        __reg_export_complete_args $previous_token
    else if __fish_seen_subcommand_from query
        __reg_query_complete_args $previous_token
    else if __fish_seen_subcommand_from save
        __reg_save_complete_args $previous_token
    end
end

complete -c reg -f -a '(__reg_complete_args)'

complete -c reg -f \
    -n 'not __fish_seen_subcommand_from add compare copy delete export import load query restore save unload' -a add \
    -d 'Add a new subkey or entry'
complete -c reg -f \
    -n 'not __fish_seen_subcommand_from add compare copy delete export import load query restore save unload' -a compare \
    -d 'Compare specified registry subkeys or entries'
complete -c reg -f \
    -n 'not __fish_seen_subcommand_from add compare copy delete export import load query restore save unload' -a copy \
    -d 'Copy a registry entry'
complete -c reg -f \
    -n 'not __fish_seen_subcommand_from add compare copy delete export import load query restore save unload' -a delete \
    -d 'Delete a subkey or entries'
complete -c reg -f \
    -n 'not __fish_seen_subcommand_from add compare copy delete export import load query restore save unload' -a export \
    -d 'Copy the specified subkeys, entries, and values of the local computer into a file'
complete -c reg -f \
    -n 'not __fish_seen_subcommand_from add compare copy delete export import load query restore save unload' -a import \
    -d 'Copy contents of a file with registry data into the local registry'
complete -c reg -f \
    -n 'not __fish_seen_subcommand_from add compare copy delete export import load query restore save unload' -a load \
    -d 'Write saved subkeys and entries into a different subkey in the registry'
complete -c reg -f \
    -n 'not __fish_seen_subcommand_from add compare copy delete export import load query restore save unload' -a query \
    -d 'Return a list of the next tier of subkeys and entries'
complete -c reg -f \
    -n 'not __fish_seen_subcommand_from add compare copy delete export import load query restore save unload' -a restore \
    -d 'Write saved subkeys and entries back'
complete -c reg -f \
    -n 'not __fish_seen_subcommand_from add compare copy delete export import load query restore save unload' -a save \
    -d 'Save a copy of specified subkeys, entries, and values of the registry in a specified file'
complete -c reg -f \
    -n 'not __fish_seen_subcommand_from add compare copy delete export import load query restore save unload' -a unload \
    -d 'Remove a section of the registry that was loaded using the reg load operation'
