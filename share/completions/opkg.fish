#completion for opkg

function __fish_opkg_no_subcommand -d 'Test if opkg has yet to be given the subcommand'
    for i in (commandline -xpc)
        if contains -- $i update upgrade install configure remove flag list list-installed list-upgradable list-changed-conffiles files search find info status download compare-versions print-architecture depends whatdepends whatdependsrec whatrecommends whatsuggests whatprovides whatconflicts whatreplaces
            return 1
        end
    end
    return 0
end

function __fish_opkg_use_package -d 'Test if opkg command should have packages as potential completion'
    for i in (commandline -xpc)
        if contains -- $i contains install search find info status download compare-versions print-architecture depends whatdepends whatdependsrec whatrecommends whatsuggests whatprovides whatconflicts whatreplaces
            return 0
        end
    end
    return 1
end

function __fish_opkg_use_package_installed -d 'Test if opkg command should have installed packages as potential completion'
    for i in (commandline -xpc)
        if contains -- $i contains upgrade configure remove flag files
            return 0
        end
    end
    return 1
end

complete -c opkg -n __fish_opkg_use_package -a '(__fish_print_opkg_packages)' -d Package

complete -c opkg -n __fish_opkg_use_package_installed -a '(__fish_print_opkg_packages --installed)' -d Package

complete -f -n __fish_opkg_no_subcommand -c opkg -a update -d 'Update list of available packages'
complete -f -n __fish_opkg_no_subcommand -c opkg -a upgrade -d 'Upgrade packages'
complete -f -n __fish_opkg_no_subcommand -c opkg -a install -d 'Install package(s)'
complete -f -n __fish_opkg_no_subcommand -c opkg -a configure -d 'Configure unpacked package(s)'
complete -f -n __fish_opkg_no_subcommand -c opkg -a remove -d 'Remove package(s)'
complete -f -n __fish_opkg_no_subcommand -c opkg -a flag -d 'Flag package(s)'
complete -f -n __fish_opkg_no_subcommand -c opkg -a list -d 'List available packages'
complete -f -n __fish_opkg_no_subcommand -c opkg -a list-installed -d 'List installed packages'
complete -f -n __fish_opkg_no_subcommand -c opkg -a list-upgradable -d 'List installed and upgradable packages'
complete -f -n __fish_opkg_no_subcommand -c opkg -a list-changed-conffiles -d 'List user modified configuration files'
complete -f -n __fish_opkg_no_subcommand -c opkg -a files -d 'List files belonging to <pkg>'
complete -f -n __fish_opkg_no_subcommand -c opkg -a search -d 'List package providing <file>'
complete -f -n __fish_opkg_no_subcommand -c opkg -a find -d 'List packages whose name or description matches <regexp>'
complete -f -n __fish_opkg_no_subcommand -c opkg -a info -d 'Display all info for <pkg>'
complete -f -n __fish_opkg_no_subcommand -c opkg -a status -d 'Display all status for <pkg>'
complete -f -n __fish_opkg_no_subcommand -c opkg -a download -d 'Download <pkg> to current directory'
complete -f -n __fish_opkg_no_subcommand -c opkg -a compare-versions -d 'compare versions using <= < > >= = << >>'
complete -f -n __fish_opkg_no_subcommand -c opkg -a print-architecture -d 'List installable package architectures'
complete -f -n __fish_opkg_no_subcommand -c opkg -a depends -d 'list depends'
complete -f -n __fish_opkg_no_subcommand -c opkg -a whatdepends -d 'list whatdepends'
complete -f -n __fish_opkg_no_subcommand -c opkg -a whatdependsrec -d 'list whatdepends recursively'
complete -f -n __fish_opkg_no_subcommand -c opkg -a whatrecommends -d 'list whatrecommends'
complete -f -n __fish_opkg_no_subcommand -c opkg -a whatsuggests -d 'list whatsuggests'
complete -f -n __fish_opkg_no_subcommand -c opkg -a whatprovides -d 'list whatprovides'
complete -f -n __fish_opkg_no_subcommand -c opkg -a whatconflicts -d 'list whatconflicts'
complete -f -n __fish_opkg_no_subcommand -c opkg -a whatreplaces -d 'list whatreplaces'

complete -c opkg -s A -d 'Query all packages not just those installed'
complete -c opkg -s V -l verbosity -d 'Set verbosity level to <level>'
complete -c opkg -s f -l conf -d 'Use <conf_file> as the opkg configuration file'
complete -c opkg -l cache -d 'Use a package cache'
complete -c opkg -s d -l dest -d 'Use <dest_name> as the root directory'
complete -c opkg -s o -l offline-root -d 'Use <dir> as the root directory for offline'
complete -c opkg -l add-arch -d 'Register architecture with given priority'
complete -c opkg -l add-dest -d 'Register destination with given path'

complete -c opkg -l force-depends -d 'Install/remove despite failed dependencies'
complete -c opkg -l force-maintainer -d 'Overwrite preexisting config files'
complete -c opkg -l force-reinstall -d 'Reinstall package(s)'
complete -c opkg -l force-overwrite -d 'Overwrite files from other package(s)'
complete -c opkg -l force-downgrade -d 'Allow opkg to downgrade packages'
complete -c opkg -l force-space -d 'Disable free space checks'
complete -c opkg -l force-postinstall -d 'Run postinstall scripts even in offline mode'
complete -c opkg -l force-remove -d 'Remove package even if prerm script fails'
complete -c opkg -l force-checksum -d 'Don\'t fail on checksum mismatches'
complete -c opkg -l noaction -d 'No action -- test only'
complete -c opkg -l download-only -d 'No action -- download only'
complete -c opkg -l nodeps -d 'Do not follow dependencies'
complete -c opkg -l nocase -d 'Perform case insensitive pattern matching'
complete -c opkg -l size -d 'Print package size when listing available packages'
complete -c opkg -l force-removal-of-dependent-packages -d 'Remove package and all dependencies'
complete -c opkg -l autoremove -d 'Remove automatically installed packages'
complete -c opkg -s t -l tmp-dir -d 'Specify tmp-dir.'
complete -c opkg -s l -l lists-dir -d 'Specify lists-dir.'
