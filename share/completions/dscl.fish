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

# Return the positional arguments (datasource, command, command args) with
# leading dashes preserved for the command.  Skips dscl option flags and the
# option values taken by -u, -P and -f.
function __fish_dscl_positionals
    set -l toks (commandline -xpc)
    set -e toks[1]
    set -l seen_ds false
    while set -q toks[1]
        switch $toks[1]
            case -u -P -f
                set -e toks[1]
                set -q toks[1]; and set -e toks[1]
            case -p -raw -plist -url -q
                set -e toks[1]
            case '-*'
                if $seen_ds
                    echo $toks[1]
                end
                set -e toks[1]
            case '*'
                set seen_ds true
                echo $toks[1]
                set -e toks[1]
        end
    end
end

# Return the Nth dscl positional argument (1 = datasource, 2 = command, …).
# No output and non-zero status if it doesn't exist.
function __fish_dscl_at
    set -l n $argv[1]
    set -l pos (__fish_dscl_positionals)
    set -q pos[$n]; and echo $pos[$n]
end

function __fish_dscl_datasource
    __fish_dscl_at 1
end

function __fish_dscl_command
    set -l cmd (__fish_dscl_at 2)
    or return 1
    string trim -l -c - -- $cmd
    return 0
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
complete -c dscl -n 'not __fish_dscl_datasource &>/dev/null' -s p -d 'Prompt for password'
complete -c dscl -n 'not __fish_dscl_datasource &>/dev/null' -s u -x -d 'Authenticate as user'
complete -c dscl -n 'not __fish_dscl_datasource &>/dev/null' -s P -x -d 'Authentication password'
complete -c dscl -n 'not __fish_dscl_datasource &>/dev/null' -s f -r -d 'Targeted local node database file path'
complete -c dscl -n 'not __fish_dscl_datasource &>/dev/null' -o raw -d "Don't strip prefix from DirectoryService API constants"
complete -c dscl -n 'not __fish_dscl_datasource &>/dev/null' -o plist -d 'Print record(s)/attribute(s) in XML plist format'
complete -c dscl -n 'not __fish_dscl_datasource &>/dev/null' -o url -d 'Print record attribute values in URL-style encoding'
complete -c dscl -n 'not __fish_dscl_datasource &>/dev/null' -s q -d 'Quiet - no interactive prompt'

# ── datasources (live, plus the well-known nodes) ────────────────────
complete -c dscl -x -n 'not __fish_dscl_datasource &>/dev/null' -a . -d 'Local directory node (shorthand)'
complete -c dscl -x -n 'not __fish_dscl_datasource &>/dev/null' -a /Local/Default -d 'Local directory node'
complete -c dscl -x -n 'not __fish_dscl_datasource &>/dev/null' -a /Search -d 'Aggregated search node'
complete -c dscl -x -n 'not __fish_dscl_datasource &>/dev/null' -a localhost -d 'Local host (no auth required)'
complete -c dscl -x -n 'not __fish_dscl_datasource &>/dev/null' -a localonly -d 'Local-plugin-only DirectoryService instance'

# ── commands (leading dash optional; offered once a datasource is set) ─
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a read -d 'Print a directory (aliases: cat .)'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a readall -d 'Print all records of a given type'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a readpl -d 'Print contents of a plist path'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a readpli -d 'Print plist-path contents at a value index'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a list -d 'List subdirectories of a directory (alias: ls)'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a search -d 'Search for records matching a pattern'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a create -d 'Create a record, property, or value (alias: mk)'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a createpl -d 'Create a plist property from a plist path'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a createpli -d 'Create a plist property at a value index'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a append -d 'Append values to an existing property'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a merge -d 'Merge values into a property (no duplicates)'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a delete -d 'Delete a record, property, or value (alias: rm)'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a deletepl -d 'Delete values from a plist path'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a deletepli -d 'Delete plist-path values at a value index'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a change -d 'Change a value of a property'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a changei -d 'Change a property value at a value index'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a diff -d 'Compare records or values at two paths'
complete -c dscl -x -n '__fish_dscl_datasource &>/dev/null; and not __fish_dscl_command &>/dev/null' -a passwd -d 'Change or set a password for a user record'

# ── command arguments: directory paths (live enumeration) ────────────
set -l path_cmds read readall readpl readpli list search create createpl \
    createpli append merge delete deletepl deletepli change changei diff passwd
complete -c dscl -x -n "contains -- (__fish_dscl_command) $path_cmds" \
    -a '(__fish_dscl_toplevel)' -d 'Record type'
complete -c dscl -x -n "contains -- (__fish_dscl_command) $path_cmds" \
    -a '(__fish_dscl_records)'
