#completion for apt-mark

function __fish_apt_no_subcommand -d 'Test if apt has yet to be given the subcommand'
    for i in (commandline -xpc)
        if contains -- $i auto manual minimize-manual hold unhold showauto showmanual showhold
            return 1
        end
    end
    return 0
end

function __fish_apt_use_package -d 'Test if apt command should have packages as potential completion'
    for i in (commandline -xpc)
        if contains -- $i contains auto manual hold unhold
            return 0
        end
    end
    return 1
end

complete -c apt-mark -n __fish_apt_use_package -a '(__fish_print_apt_packages)' -d Package

complete -c apt-mark -s h -l help -d 'Display help and exit'
complete -f -n __fish_apt_no_subcommand -c apt-mark -a auto -d 'Mark a package as automatically installed'
complete -f -n __fish_apt_no_subcommand -c apt-mark -a manual -d 'Mark a package as manually installed'
complete -f -n __fish_apt_no_subcommand -c apt-mark -a minimize-manual -d 'Mark all dependencies of meta packages as auto'
complete -f -n __fish_apt_no_subcommand -c apt-mark -a hold -d 'Hold a package, prevent automatic installation or removal'
complete -f -n __fish_apt_no_subcommand -c apt-mark -a unhold -d 'Cancel a hold on a package'
complete -f -n __fish_apt_no_subcommand -c apt-mark -a showauto -d 'Show automatically installed packages'
complete -f -n __fish_apt_no_subcommand -c apt-mark -a showmanual -d 'Show manually installed packages'
complete -f -n __fish_apt_no_subcommand -c apt-mark -a showhold -d 'Show held packages'
complete -c apt-mark -s v -l version -d 'Display version and exit'
complete -r -c apt-mark -s c -l config-file -d 'Specify a config file'
complete -r -c apt-mark -s o -l option -d 'Set a config option'
complete -c apt-mark -l color -d 'Turn colors on'
complete -c apt-mark -l no-color -d 'Turn colors off'
complete -r -c apt-mark -s f -l file -d 'Write package statistics to a file'
