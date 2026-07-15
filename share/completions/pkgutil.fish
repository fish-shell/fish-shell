function __fish_pkgutil_primary_cmd
    set -l toks (commandline -xpc)
    set -e toks[1]
    set -l primaries packages pkgs pkgs-plist files export-plist pkg-info pkg-info-plist forget \
        learn pkg-groups groups groups-plist group-pkgs file-info file-info-plist expand flatten \
        bom payload-files check-signature
    for t in $toks
        set -l stripped (string replace -r '^--' '' -- $t)
        set stripped (string replace -r '=.*' '' -- $stripped)
        if contains -- $stripped $primaries
            echo $stripped
            return 0
        end
    end
    return 1
end

function __fish_pkgutil_no_primary_cmd
    not __fish_pkgutil_primary_cmd >/dev/null
end

function __fish_pkgutil_pkg_ids
    pkgutil --pkgs
end

function __fish_pkgutil_group_ids
    pkgutil --groups
end

complete -c pkgutil -l help -s h -d 'Show a brief summary of commands and usage'
complete -c pkgutil -l force -s f -d 'Do not ask for confirmation before destructive operations'
complete -c pkgutil -l verbose -s v -d 'Output in human-readable format with extra context'
complete -c pkgutil -l volume -d 'Perform operations on the specified volume or home directory' -x -a '(__fish_complete_directories)'
complete -c pkgutil -l only-files -d 'List only files (not directories) in --files listing'
complete -c pkgutil -l only-dirs -d 'List only directories (not files) in --files listing'
complete -c pkgutil -l regexp -d 'Match package-id arguments as a regular expression'
complete -c pkgutil -l edit-pkg -d 'Specify existing receipt to be modified in-place by --learn' -x -a '(__fish_pkgutil_pkg_ids)'
complete -c pkgutil -d 'List all installed package IDs on the volume' -n __fish_pkgutil_no_primary_cmd -l packages
complete -c pkgutil -d 'List all installed package IDs on the volume' -n __fish_pkgutil_no_primary_cmd -l pkgs
complete -c pkgutil -d 'List all installed package IDs in plist format' -n __fish_pkgutil_no_primary_cmd -l pkgs-plist
complete -c pkgutil -d 'List all files installed under the given package-id' -n __fish_pkgutil_no_primary_cmd -l files -x
complete -c pkgutil -d 'Print all receipt information about a package-id in plist format' -n __fish_pkgutil_no_primary_cmd -l export-plist -x
complete -c pkgutil -d 'Print extended information about a package-id' -n __fish_pkgutil_no_primary_cmd -l pkg-info -x
complete -c pkgutil -d 'Print extended information about a package-id in plist format' -n __fish_pkgutil_no_primary_cmd -l pkg-info-plist -x
complete -c pkgutil -d 'Discard all receipt data about a package-id (does not remove files)' -n __fish_pkgutil_no_primary_cmd -l forget -x
complete -c pkgutil -d 'Update ACLs of the given path in the receipt set by --edit-pkg' -n __fish_pkgutil_no_primary_cmd -l learn
complete -c pkgutil -d 'List all package groups a package-id is a member of' -n __fish_pkgutil_no_primary_cmd -l pkg-groups -x
complete -c pkgutil -d 'List all package groups on the volume' -n __fish_pkgutil_no_primary_cmd -l groups
complete -c pkgutil -d 'List all package groups on the volume in plist format' -n __fish_pkgutil_no_primary_cmd -l groups-plist
complete -c pkgutil -d 'List all packages that are members of a group-id' -n __fish_pkgutil_no_primary_cmd -l group-pkgs -x
complete -c pkgutil -d 'Show metadata known about a path' -n __fish_pkgutil_no_primary_cmd -l file-info
complete -c pkgutil -d 'Show metadata known about a path in plist format' -n __fish_pkgutil_no_primary_cmd -l file-info-plist
complete -c pkgutil -d 'Expand a flat package at pkg-path into a new directory at dir-path' -n __fish_pkgutil_no_primary_cmd -l expand
complete -c pkgutil -d 'Flatten a directory into a new flat package' -n __fish_pkgutil_no_primary_cmd -l flatten
complete -c pkgutil -d 'Extract BOM files from a flat package into /tmp and return filenames' -n __fish_pkgutil_no_primary_cmd -l bom
complete -c pkgutil -d 'List files archived within the payload of an uninstalled flat package' -n __fish_pkgutil_no_primary_cmd -l payload-files
complete -c pkgutil -d 'Check the validity and trust of the signature on a package' -n __fish_pkgutil_no_primary_cmd -l check-signature
set -l pkg_id_cmds files export-plist pkg-info pkg-info-plist forget pkg-groups
complete -c pkgutil -x -n "contains -- (__fish_pkgutil_primary_cmd) $pkg_id_cmds" -a '(__fish_pkgutil_pkg_ids)'
complete -c pkgutil -x -n "contains -- (__fish_pkgutil_primary_cmd) group-pkgs" -a '(__fish_pkgutil_group_ids)'
