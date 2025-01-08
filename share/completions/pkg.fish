# Completions for pkgng package manager

# Solaris has a thing called "pkg"; it works quite differently and spews errors when called here.
# There are multiple SunOS-derived distributions and not all of them have `SunOS` in their name (and
# some of them also use pkgsrc and have a `pkg`).
#
# Additionally, this particular script is intended to complete the pkgng "Next Generation" package
# manager initially developed for FreeBSD though now available on a few other BSDs. From here on
# out, maintainers can assume we are specifically talking about the (Free)BSD `pkg` command being
# executed on a BSD system, rather than just work with "not SunOS".
if ! uname | string match -irq bsd
    exit
end

function __fish_pkg_is
    for option in $argv
        if contains $option (commandline -pxc)
            return 0
        end
    end
    return 1
end

function __fish_pkg_subcommand
    set -l skip_next 1
    for token in (commandline -xpc)
        if test $skip_next = 1
            set skip_next 0
            continue
        end
        switch $token
            # Option parser allows shortened long options
            case '--*=*'
                continue
            case -{o,j,c,r,C,R} --op\* --j\* --ch\* --ro\* --co\* --re\*
                set skip_next 1
                continue
            case '-*'
                continue
        end
        return 1
    end
    return 0
end

complete -c pkg -n __fish_pkg_subcommand -s v -l version -d "Display version and exit"
complete -c pkg -n __fish_pkg_subcommand -s d -l debug -d "Show debug information"
complete -c pkg -n __fish_pkg_subcommand -s l -l list -d "List subcommands"
complete -c pkg -n __fish_pkg_subcommand -x -s o -l option -d "Set configuration option"
complete -c pkg -n __fish_pkg_subcommand -s N -d "Run sanity test"
complete -c pkg -n __fish_pkg_subcommand -x -s j -l jail -d "Run package manager within jail"
complete -c pkg -n __fish_pkg_subcommand -r -s c -l chroot -d "Run package manager within chroot"
complete -c pkg -n __fish_pkg_subcommand -r -s r -l rootdir -d "Install packages in specified root"
complete -c pkg -n __fish_pkg_subcommand -r -s C -l config -d "Use configuration file"
complete -c pkg -n __fish_pkg_subcommand -r -s R -l repo-conf-dir -d "Set repository configuration directory"
complete -c pkg -n __fish_pkg_subcommand -s 4 -d "Use IPv4"
complete -c pkg -n __fish_pkg_subcommand -s 6 -d "Use IPv6"

complete -c pkg -n __fish_pkg_subcommand -xa add -d "Install package file"
complete -c pkg -n __fish_pkg_subcommand -xa alias -d "List the command line aliases"
complete -c pkg -n __fish_pkg_subcommand -xa annotate -d "Modify annotations on packages"
complete -c pkg -n __fish_pkg_subcommand -xa audit -d "Audit installed packages"
complete -c pkg -n __fish_pkg_subcommand -xa autoremove -d "Delete unneeded packages"
complete -c pkg -n __fish_pkg_subcommand -xa backup -d "Dump package database"
complete -c pkg -n __fish_pkg_subcommand -xa bootstrap -d "Install pkg(8) from remote repository"
complete -c pkg -n __fish_pkg_subcommand -xa check -d "Check installed packages"
complete -c pkg -n __fish_pkg_subcommand -xa clean -d "Clean local cache"
complete -c pkg -n __fish_pkg_subcommand -xa convert -d "Convert package from pkg_add format"
complete -c pkg -n __fish_pkg_subcommand -xa create -d "Create a package"
complete -c pkg -n __fish_pkg_subcommand -xa delete -d "Remove a package"
complete -c pkg -n __fish_pkg_subcommand -xa fetch -d "Download a remote package"
complete -c pkg -n __fish_pkg_subcommand -xa help -d "Display help for command"
complete -c pkg -n __fish_pkg_subcommand -xa info -d "List installed packages"
complete -c pkg -n __fish_pkg_subcommand -xa install -d "Install packages"
complete -c pkg -n __fish_pkg_subcommand -xa list -d "List files belonging to package(s)"
complete -c pkg -n __fish_pkg_subcommand -xa lock -d "Prevent package modification"
complete -c pkg -n __fish_pkg_subcommand -xa plugins -d "List package manager plugins"
complete -c pkg -n __fish_pkg_subcommand -xa query -d "Query installed packages"
complete -c pkg -n __fish_pkg_subcommand -xa register -d "Register a package in a database"
complete -c pkg -n __fish_pkg_subcommand -xa remove -d "Remove a package"
complete -c pkg -n __fish_pkg_subcommand -xa repo -d "Create package repository"
complete -c pkg -n __fish_pkg_subcommand -xa rquery -d "Query information for remote repositories"
complete -c pkg -n __fish_pkg_subcommand -xa search -d "Find packages"
complete -c pkg -n __fish_pkg_subcommand -xa set -d "Modify package information in database"
complete -c pkg -n __fish_pkg_subcommand -xa shell -d "Open a SQLite shell"
complete -c pkg -n __fish_pkg_subcommand -xa shlib -d "Display packages linking to shared library"
complete -c pkg -n __fish_pkg_subcommand -xa show -d "Show information about package(s)"
complete -c pkg -n __fish_pkg_subcommand -xa stats -d "Display package statistics"
complete -c pkg -n __fish_pkg_subcommand -xa unlock -d "Stop preventing package modification"
complete -c pkg -n __fish_pkg_subcommand -xa update -d "Update remote repositories"
complete -c pkg -n __fish_pkg_subcommand -xa updating -d "Display UPDATING entries"
complete -c pkg -n __fish_pkg_subcommand -xa upgrade -d "Upgrade packages"
complete -c pkg -n __fish_pkg_subcommand -xa version -d "Show package versions"
complete -c pkg -n __fish_pkg_subcommand -xa which -d "Check which package provides a file"

complete -c pkg -n __fish_pkg_subcommand -xa '(__fish_pkg_aliases)'

# add
complete -c pkg -n '__fish_pkg_is add install' -s A -l automatic -d "Mark packages as automatic"
complete -c pkg -n '__fish_pkg_is add bootstrap install' -s f -l force -d "Force installation even when installed"
complete -c pkg -n '__fish_pkg_is add' -s I -l no-scripts -d "Disable installation scripts"
complete -c pkg -n '__fish_pkg_is add' -s M -l accept-missing -d "Force installation with missing dependencies"
complete -c pkg -n '__fish_pkg_is add alias autoremove clean delete remove install update' -s q -l quiet -d "Force quiet output"

# alias
complete -c pkg -n '__fish_pkg_is alias' -xa '(pkg alias -lq)'
complete -c pkg -n '__fish_pkg_is alias' -s l -l list -d "Print all aliases without their pkg(8) arguments"

# autoremove
complete -c pkg -n '__fish_pkg_is autoremove clean delete remove install upgrade' -s n -l dry-run -d "Do not make changes"
complete -c pkg -n '__fish_pkg_is autoremove clean delete remove install' -s y -l yes -d "Assume yes when asked for confirmation"

# bootstrap
complete -c pkg -n '__fish_pkg_is bootstrap' -f

# check
set -l has_check_opt '__fish_contains_opt -s B shlibs -s d dependencies -s s checksums -s r recompute'
set -l has_all_opt '__fish_contains_opt -s a all'
complete -c pkg -n "__fish_pkg_is check" -f
complete -c pkg -n "__fish_pkg_is check; and not $has_check_opt" -xa "-B -d -s -r"
complete -c pkg -n "__fish_pkg_is check; and not $has_check_opt" -s B -l shlibs -d "Regenerate library dependency metadata"
complete -c pkg -n "__fish_pkg_is check; and not $has_check_opt" -s d -l dependencies -d "Check for and install missing dependencies"
complete -c pkg -n "__fish_pkg_is check; and not $has_check_opt" -s r -l recompute -d "Recalculate and set the checksums of installed packages"
complete -c pkg -n "__fish_pkg_is check; and not $has_check_opt" -s s -l checksums -d "Detect installed packages with invalid checksums"
complete -c pkg -n "__fish_pkg_is check; and $has_check_opt" -s n -l dry-run -d "Do not make changes"
complete -c pkg -n "__fish_pkg_is check; and $has_check_opt" -s q -l quiet -d "Force quiet output"
complete -c pkg -n "__fish_pkg_is check; and $has_check_opt" -s v -l verbose -d "Provide verbose output"
complete -c pkg -n "__fish_pkg_is check; and $has_check_opt" -s y -l yes -d "Assume yes when asked for confirmation"
complete -c pkg -n "__fish_pkg_is check; and $has_check_opt; and not $has_all_opt" -xa '(pkg query "%n")'
complete -c pkg -n "__fish_pkg_is check; and $has_check_opt; and not $has_all_opt" -s a -l all -d "Process all packages"
complete -c pkg -n "__fish_pkg_is check; and $has_check_opt; and not $has_all_opt" -s C -l case-sensitive -d "Case sensitive packages"
complete -c pkg -n "__fish_pkg_is check; and $has_check_opt; and not $has_all_opt" -s g -l glob -d "Treat the package name as shell glob"
complete -c pkg -n "__fish_pkg_is check; and $has_check_opt; and not $has_all_opt" -s i -l case-insensitive -d "Case insensitive packages"
complete -c pkg -n "__fish_pkg_is check; and $has_check_opt; and not $has_all_opt" -s x -l regex -d "Treat the package name as regular expression"

# clean
complete -c pkg -n '__fish_pkg_is clean' -s a -l all -d "Delete all cached packages"

# delete/remove
complete -c pkg -n '__fish_pkg_is delete remove upgrade' -xa '(pkg query "%n")'
complete -c pkg -n '__fish_pkg_is delete remove' -s a -l all -d 'Delete all installed packages'
complete -c pkg -n '__fish_pkg_is delete remove install upgrade' -s C -l case-sensitive -d "Case sensitive packages"
complete -c pkg -n '__fish_pkg_is delete remove' -s D -l no-deinstall-script -d "Disable deinstallation scripts"
complete -c pkg -n '__fish_pkg_is delete remove' -s f -l force -d "Force package removal"
complete -c pkg -n '__fish_pkg_is delete remove install upgrade' -s g -l glob -d "Treat the package name as shell glob"
complete -c pkg -n '__fish_pkg_is delete remove install upgrade' -s i -l case-insensitive -d "Case insensitive packages"
complete -c pkg -n '__fish_pkg_is delete remove' -s R -l recursive -d "Remove recursively"
complete -c pkg -n '__fish_pkg_is delete remove install upgrade' -s x -l regex -d "Treat the package name as regular expression"

# install
complete -c pkg -n '__fish_pkg_is install' -xa '(pkg rquery -U "%n")'
complete -c pkg -n '__fish_pkg_is install' -s I -l no-install-scripts -d "Disable installation scripts"
complete -c pkg -n '__fish_pkg_is install' -s M -l ignore-missing -d "Force installation with missing dependencies"
complete -c pkg -n '__fish_pkg_is install upgrade' -s F -l fetch-only -d "Do not perform actual installation"
complete -c pkg -n '__fish_pkg_is install' -s R -l from-root -d "Reinstall packages required by this package"
complete -c pkg -n '__fish_pkg_is install update' -x -s r -l repository -d "Use only a given repository"
complete -c pkg -n '__fish_pkg_is install upgrade' -s U -l no-repo-update -d "Do not automatically update database"

# info
complete -c pkg -n '__fish_pkg_is info' -xa '(pkg query "%n")'

# show
complete -c pkg -n '__fish_pkg_is show' -xa '(pkg query "%n")'

# list
complete -c pkg -n '__fish_pkg_is list' -xa '(pkg query "%n")'

# update
complete -c pkg -n '__fish_pkg_is add update' -s f -l force -d "Force a full download of a repository"

# alias 
set -l with_package_names all-depends annotations build-depends cinfo comment csearch desc iinfo isearch \
    list options origin provided-depends roptions shared-depends show size

for alias in (pkg alias -lq)
    if contains $with_package_names $alias
        complete -c pkg -n "__fish_pkg_is $alias" -xa '(pkg query "%n")'
    end
end

function __fish_pkg_aliases
    for alias in (pkg alias -q)
        echo $alias | read -l name description

        switch $name
            case all-depends
                set description 'Display all dependencies for a given package'

            case annotations
                set description 'Display any annotations added to the package'

            case build-depends
                set description 'Display build dependencies for a given package'

            case cinfo
                set description 'Display install package matching case-sensitve regex'

            case comment
                set description 'Display comment off a package'

            case csearch
                set description 'Displays package using case-sensitive search'

            case desc
                set description 'Show package description'

            case iinfo
                set description 'Display install package matching case-insensitve regex'

            case isearch
                set description 'Finds package using case-insensitive search'

            case prime-list
                set description 'Displays names of all manually installed packages'

            case prime-origins
                set description 'Displays origin of all manually installed packages'

            case leaf
                set description 'Lists all leaf packages'

            case list
                set description 'Display all files from an installed package'

            case noauto
                set description 'Displays all non automatically installed packages'

            case options
                set description 'Display options of a installed package'

            case origin
                set description 'Shows origin of a package'

            case provided-depends
                set description 'Display all shared libraries provided by package'

            case raw
                set description 'Display the full manifest for a package'

            case required-depends
                set description 'Display the list of packages which require this package'

            case roptions
                set description 'Display options of a package for the default repository'

            case shared-depends
                set description 'Display all shared libraries used by package'

            case show
                set description 'Display full information including lock status for a package'

            case size
                set description 'Display the total size of files installed by a package'

            case '*'
                set description "alias: $description"

        end

        printf '%s\t%s\n' $name $description
    end

end
