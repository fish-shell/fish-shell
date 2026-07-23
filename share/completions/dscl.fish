# dscl - Directory Service command line utility (man 1 dscl)
#
# Grammar: dscl [options] [datasource [command [args]]]
# Options are leading flags (-p -u -P -f -raw -plist -url -q). The first
# non-option token is the datasource (a node like `.`, /Local/Default,
# /Search, or a host). The token after the datasource is the command; leading
# dashes are optional for all commands, so both `read` and `-read` are valid.
# Most commands take a directory path as their first operand, which we
# enumerate live via the fast, unprivileged `dscl . -list <path>`.

# ── command-line introspection ───────────────────────────────────────
# Print the datasource (first non-option token after the command name), or
# nothing if none has been entered yet.
function __fish_dscl_datasource
    set -l toks (commandline -opc)
    set -e toks[1]
    for t in $toks
        # -u and -P take a value argument; skip the following token.
        switch $t
            case -u -P -f
                set -e toks[1]
                continue
        end
        if not string match -q -- '-*' $t
            echo $t
            return 0
        end
    end
    return 1
end

# True when no datasource token has been entered yet.
function __fish_dscl_no_datasource
    not __fish_dscl_datasource >/dev/null 2>&1
end

# Print the command (first bare token after the datasource), without any
# leading dash; nothing if no command has been chosen yet.
function __fish_dscl_command
    set -l toks (commandline -opc)
    set -e toks[1]
    set -l seen_ds
    for t in $toks
        switch $t
            case -u -P -f
                # value-taking option: this token is the option, value follows
                set seen_ds skip
                continue
        end
        if test "$seen_ds" = skip
            set -e seen_ds
            continue
        end
        if string match -q -- '-*' $t
            # Leading-dash command form: strip the dash and treat as command,
            # but only once the datasource has been seen.
            if set -q __fish_dscl_ds_found
                string trim -l -c - -- $t
                return 0
            end
            continue
        end
        if not set -q __fish_dscl_ds_found
            set -g __fish_dscl_ds_found 1
            continue
        end
        set -e __fish_dscl_ds_found
        echo $t
        return 0
    end
    set -e __fish_dscl_ds_found
    return 1
end

# True when a datasource is present but no command has been chosen yet.
function __fish_dscl_need_command
    not __fish_dscl_no_datasource
    and not __fish_dscl_command >/dev/null 2>&1
end

# ── live enumerators (fast, unprivileged, degrade to empty) ──────────
# Top-level directories (record types) under the local node: /Users, /Groups,
# /Computers, etc. `dscl . -list /` is fast and needs no privileges.
function __fish_dscl_toplevel
    dscl . -list / 2>/dev/null
end

# Records under whatever path is already on the command line, so that after
# `dscl . read /Users` we can offer /Users/<name>. We only enumerate when the
# in-progress path token names an existing listable directory.
function __fish_dscl_records
    set -l tok (commandline -ct)
    set -l base (string replace -r '/[^/]*$' '' -- $tok)
    test -z "$base"; and set base /
    set -l prefix $base
    test "$base" = /; and set prefix ''
    for child in (dscl . -list $base 2>/dev/null)
        echo $prefix/$child
    end
end

# ── options ──────────────────────────────────────────────────────────
complete -c dscl -n __fish_dscl_no_datasource -s p -d 'Prompt for password'
complete -c dscl -n __fish_dscl_no_datasource -s u -x -d 'Authenticate as user'
complete -c dscl -n __fish_dscl_no_datasource -s P -x -d 'Authentication password'
complete -c dscl -n __fish_dscl_no_datasource -s f -r -d 'Targeted local node database file path'
complete -c dscl -n __fish_dscl_no_datasource -o raw -d "Don't strip prefix from DirectoryService API constants"
complete -c dscl -n __fish_dscl_no_datasource -o plist -d 'Print record(s)/attribute(s) in XML plist format'
complete -c dscl -n __fish_dscl_no_datasource -o url -d 'Print record attribute values in URL-style encoding'
complete -c dscl -n __fish_dscl_no_datasource -s q -d 'Quiet - no interactive prompt'

# ── datasources (live, plus the well-known nodes) ────────────────────
complete -c dscl -x -n __fish_dscl_no_datasource -a . -d 'Local directory node (shorthand)'
complete -c dscl -x -n __fish_dscl_no_datasource -a /Local/Default -d 'Local directory node'
complete -c dscl -x -n __fish_dscl_no_datasource -a /Search -d 'Aggregated search node'
complete -c dscl -x -n __fish_dscl_no_datasource -a localhost -d 'Local host (no auth required)'
complete -c dscl -x -n __fish_dscl_no_datasource -a localonly -d 'Local-plugin-only DirectoryService instance'

# ── commands (leading dash optional; offered once a datasource is set) ─
complete -c dscl -x -n __fish_dscl_need_command -a read -d 'Print a directory (aliases: cat .)'
complete -c dscl -x -n __fish_dscl_need_command -a readall -d 'Print all records of a given type'
complete -c dscl -x -n __fish_dscl_need_command -a readpl -d 'Print contents of a plist path'
complete -c dscl -x -n __fish_dscl_need_command -a readpli -d 'Print plist-path contents at a value index'
complete -c dscl -x -n __fish_dscl_need_command -a list -d 'List subdirectories of a directory (alias: ls)'
complete -c dscl -x -n __fish_dscl_need_command -a search -d 'Search for records matching a pattern'
complete -c dscl -x -n __fish_dscl_need_command -a create -d 'Create a record, property, or value (alias: mk)'
complete -c dscl -x -n __fish_dscl_need_command -a createpl -d 'Create a plist property from a plist path'
complete -c dscl -x -n __fish_dscl_need_command -a createpli -d 'Create a plist property at a value index'
complete -c dscl -x -n __fish_dscl_need_command -a append -d 'Append values to an existing property'
complete -c dscl -x -n __fish_dscl_need_command -a merge -d 'Merge values into a property (no duplicates)'
complete -c dscl -x -n __fish_dscl_need_command -a delete -d 'Delete a record, property, or value (alias: rm)'
complete -c dscl -x -n __fish_dscl_need_command -a deletepl -d 'Delete values from a plist path'
complete -c dscl -x -n __fish_dscl_need_command -a deletepli -d 'Delete plist-path values at a value index'
complete -c dscl -x -n __fish_dscl_need_command -a change -d 'Change a value of a property'
complete -c dscl -x -n __fish_dscl_need_command -a changei -d 'Change a property value at a value index'
complete -c dscl -x -n __fish_dscl_need_command -a diff -d 'Compare records or values at two paths'
complete -c dscl -x -n __fish_dscl_need_command -a passwd -d 'Change or set a password for a user record'

# ── command arguments: directory paths (live enumeration) ────────────
set -l path_cmds read readall readpl readpli list search create createpl \
    createpli append merge delete deletepl deletepli change changei diff passwd
complete -c dscl -x -n "contains -- (__fish_dscl_command) $path_cmds" \
    -a '(__fish_dscl_toplevel)' -d 'Record type'
complete -c dscl -x -n "contains -- (__fish_dscl_command) $path_cmds" \
    -a '(__fish_dscl_records)'
