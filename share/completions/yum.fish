#
# Completions for the yum command
#

# All yum commands

# Test if completing using package names is appropriate
function __fish_yum_package_ok
    for i in (commandline -pxc)
        if contains -- $i update upgrade remove erase install reinstall
            return 0
        end
    end
    return 1
end

complete -c yum -n __fish_use_subcommand -xa install -d "Install the latest version of a package"
complete -c yum -n __fish_use_subcommand -xa 'update upgrade' -d "Update specified packages (defaults to all packages)"
complete -c yum -n __fish_use_subcommand -xa check-update -d "Print list of available updates"
complete -c yum -n __fish_use_subcommand -xa 'remove erase' -d "Remove the specified packages and packages that depend on them"
complete -c yum -n __fish_use_subcommand -xa list -d "List available packages"
complete -c yum -n __fish_use_subcommand -xa info -d "Describe available packages"
complete -c yum -n __fish_use_subcommand -xa 'provides whatprovides' -d "Find package providing a feature or file"
complete -c yum -n __fish_use_subcommand -xa search -d "find packages matching description regexp"
complete -c yum -n __fish_use_subcommand -xa clean -d "Clean up cache directory"
complete -c yum -n __fish_use_subcommand -xa generate-rss -d "Generate rss changelog"

complete -c yum -n __fish_yum_package_ok -a "(__fish_print_rpm_packages)"

complete -c yum -s h -l help -d "Display help and exit"
complete -c yum -s y -d "Assume yes to all questions"
complete -c yum -s c -d "Configuration file" -r
complete -c yum -s d -d "Set debug level" -x
complete -c yum -s e -d "Set error level" -x
complete -c yum -s t -l tolerant -d "Be tolerant of errors in commandline"
complete -c yum -s R -d "Set maximum delay between commands" -x
complete -c yum -s c -d "Run commands from cache"
complete -c yum -l version -d "Display version and exit"
complete -c yum -l installroot -d "Specify installroot" -r
complete -c yum -l enablerepo -d "Enable repository" -r
complete -c yum -l disablerepo -d "Disable repository" -r
complete -c yum -l obsoletes -d "Enables obsolets processing logic"
complete -c yum -l rss-filename -d "Output rss-data to file" -r
complete -c yum -l exclude -d "Exclude specified package from updates" -a "(__fish_print_rpm_packages)"

complete -c yum -n 'contains list (commandline -pxc)' -a all -d 'List all packages'
complete -c yum -n 'contains list (commandline -pxc)' -a available -d 'List packages available for installation'
complete -c yum -n 'contains list (commandline -pxc)' -a updates -d 'List packages with updates available'
complete -c yum -n 'contains list (commandline -pxc)' -a installed -d 'List installed packages'
complete -c yum -n 'contains list (commandline -pxc)' -a extras -d 'List packages not available in repositories'
complete -c yum -n 'contains list (commandline -pxc)' -a obsoletes -d 'List packages that are obsoleted by packages in repositories'

complete -c yum -n 'contains clean (commandline -pxc)' -x -a packages -d 'Delete cached package files'
complete -c yum -n 'contains clean (commandline -pxc)' -x -a headers -d 'Delete cached header files'
complete -c yum -n 'contains clean (commandline -pxc)' -x -a all -d 'Delete all cache contents'
