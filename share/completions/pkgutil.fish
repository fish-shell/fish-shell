# pkgutil - query and manipulate macOS installer packages and receipts (man 1 pkgutil)
#
# pkgutil uses double-dash long flags as both options and primary commands.
# Options (--volume, --verbose, etc.) modify the primary commands.
# Primary commands either take a package-id, a path, or nothing.

# ── command-line introspection ────────────────────────────────────────────────
# Return the primary command flag seen on the current commandline (without dashes),
# or exit non-zero if none has been chosen yet.
function __fish_pkgutil_primary_cmd
    set -l toks (commandline -opc)
    set -e toks[1]
    # Primary commands are those that are not pure modifier options
    set -l primaries \
        packages pkgs pkgs-plist files export-plist pkg-info pkg-info-plist \
        forget learn pkg-groups groups groups-plist group-pkgs \
        file-info file-info-plist \
        expand flatten bom payload-files check-signature
    for t in $toks
        set -l stripped (string replace -r '^--' '' -- $t)
        # Handle --pkgs=REGEXP form: strip the =... part
        set stripped (string replace -r '=.*' '' -- $stripped)
        if contains -- $stripped $primaries
            echo $stripped
            return 0
        end
    end
    return 1
end

# True when no primary command has been seen yet.
function __fish_pkgutil_no_primary_cmd
    not __fish_pkgutil_primary_cmd >/dev/null 2>&1
end

# ── live enumerators ──────────────────────────────────────────────────────────
# List installed package IDs (fast, unprivileged).
function __fish_pkgutil_pkg_ids
    pkgutil --pkgs 2>/dev/null
end

# List package group IDs (fast, unprivileged).
function __fish_pkgutil_group_ids
    pkgutil --groups 2>/dev/null
end

# ── global modifier options ───────────────────────────────────────────────────
# These may appear before or alongside any primary command.
complete -c pkgutil -l help -s h -f -d 'Show a brief summary of commands and usage'
complete -c pkgutil -l force -s f -f -d 'Do not ask for confirmation before destructive operations'
complete -c pkgutil -l verbose -s v -f -d 'Output in human-readable format with extra context'
complete -c pkgutil -l volume -d 'Perform operations on the specified volume or home directory' -x -a '(__fish_complete_directories)'
complete -c pkgutil -l only-files -f -d 'List only files (not directories) in --files listing'
complete -c pkgutil -l only-dirs -f -d 'List only directories (not files) in --files listing'
complete -c pkgutil -l regexp -f -d 'Match package-id arguments as a regular expression'
complete -c pkgutil -l edit-pkg -d 'Specify existing receipt to be modified in-place by --learn' -x -a '(__fish_pkgutil_pkg_ids)'

# ── primary commands: receipt database ───────────────────────────────────────
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l packages -f -d 'List all installed package IDs on the volume'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l pkgs -f -d 'List all installed package IDs on the volume'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l pkgs-plist -f -d 'List all installed package IDs in plist format'

complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l files -x -d 'List all files installed under the given package-id'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l export-plist -x -d 'Print all receipt information about a package-id in plist format'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l pkg-info -x -d 'Print extended information about a package-id'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l pkg-info-plist -x -d 'Print extended information about a package-id in plist format'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l forget -x -d 'Discard all receipt data about a package-id (does not remove files)'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l learn -d 'Update ACLs of the given path in the receipt set by --edit-pkg'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l pkg-groups -x -d 'List all package groups a package-id is a member of'

complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l groups -f -d 'List all package groups on the volume'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l groups-plist -f -d 'List all package groups on the volume in plist format'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l group-pkgs -x -d 'List all packages that are members of a group-id'

complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l file-info -d 'Show metadata known about a path'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l file-info-plist -d 'Show metadata known about a path in plist format'

# ── primary commands: flat package file operations ───────────────────────────
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l expand -d 'Expand a flat package at pkg-path into a new directory at dir-path'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l flatten -d 'Flatten a directory into a new flat package'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l bom -d 'Extract BOM files from a flat package into /tmp and return filenames'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l payload-files -d 'List files archived within the payload of an uninstalled flat package'
complete -c pkgutil -n __fish_pkgutil_no_primary_cmd \
    -l check-signature -d 'Check the validity and trust of the signature on a package'

# ── argument completions for primary commands that take a package-id ──────────
set -l pkg_id_cmds files export-plist pkg-info pkg-info-plist forget pkg-groups
complete -c pkgutil -x \
    -n "contains -- (__fish_pkgutil_primary_cmd) $pkg_id_cmds" \
    -a '(__fish_pkgutil_pkg_ids)'

# --group-pkgs takes a group-id
complete -c pkgutil -x \
    -n "contains -- (__fish_pkgutil_primary_cmd) group-pkgs" \
    -a '(__fish_pkgutil_group_ids)'
