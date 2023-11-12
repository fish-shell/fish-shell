#
# Completions for the dnf command
#

function __dnf_list_installed_packages
    dnf repoquery --cacheonly "$cur*" --qf "%{name}" --installed </dev/null
end

function __dnf_list_available_packages
    set -l tok (commandline -ct | string collect)
    set -l files (__fish_complete_suffix .rpm)
    if string match -q -- '*/*' $tok
        # Fast path - package names can't contain slashes, so show files.
        string join -- \n $files
        return
    end
    set -l results
    # dnf --cacheonly list --available gives a list of non-installed packages dnf is aware of,
    # but it is slow as molasses. Unfortunately, sqlite3 is not available oob (Fedora Server 32).
    if type -q sqlite3
        # This schema is bad, there is only a "pkg" field with the full
        #    packagename-version-release.fedorarelease.architecture
        # tuple. We are only interested in the packagename.
        set results (sqlite3 /var/cache/dnf/packages.db "SELECT pkg FROM available WHERE pkg LIKE '$tok%'" 2>/dev/null |
            string replace -r -- '-[^-]*-[^-]*$' '')
    else
        # In some cases dnf will ask for input (e.g. to accept gpg keys).
        # Connect it to /dev/null to try to stop it.
        set results (dnf repoquery --cacheonly "$tok*" --qf "%{name}" --available </dev/null 2>/dev/null)
    end
    if set -q results[1]
        set results (string match -r -- '.*\\.rpm$' $files) $results
    else
        set results $files
    end
    string join \n $results
end

function __dnf_list_transactions
    if type -q sqlite3
        sqlite3 /var/lib/dnf/history.sqlite "SELECT id, cmdline FROM trans" 2>/dev/null | string replace "|" \t
    end
end

# Alias
complete -c dnf -n __fish_use_subcommand -xa alias -d "Manage aliases"
complete -c dnf -n "__fish_seen_subcommand_from alias" -xa add -d "Add a new alias"
complete -c dnf -n "__fish_seen_subcommand_from alias" -xa list -d "Lists all defined aliases"
complete -c dnf -n "__fish_seen_subcommand_from alias" -xa delete -d "Delete an alias"

# Autoremove
complete -c dnf -n __fish_use_subcommand -xa autoremove -d "Removes unneeded packages"
complete -c dnf -n "__fish_seen_subcommand_from autoremove" -xa "(__dnf_list_installed_packages)"

# Check
complete -c dnf -n __fish_use_subcommand -xa check -d "Check for problems in packagedb"
complete -c dnf -n "__fish_seen_subcommand_from check" -l dependencies -d "Checks dependencies"
complete -c dnf -n "__fish_seen_subcommand_from check" -l duplicates -d "Checks duplicates"
complete -c dnf -n "__fish_seen_subcommand_from check" -l obsoleted -d "Checks obsoleted"
complete -c dnf -n "__fish_seen_subcommand_from check" -l provides -d "Checks provides"

# Check-Update
complete -c dnf -n __fish_use_subcommand -xa check-update -d "Checks for updates"
complete -c dnf -n "__fish_seen_subcommand_from check-update" -l changelogs

# Clean
complete -c dnf -n __fish_use_subcommand -xa clean -d "Clean up cache directory"
complete -c dnf -n "__fish_seen_subcommand_from clean" -xa dbcache -d "Removes the database cache"
complete -c dnf -n "__fish_seen_subcommand_from clean" -xa expire-cache -d "Marks the repository metadata expired"
complete -c dnf -n "__fish_seen_subcommand_from clean" -xa metadata -d "Removes repository metadata"
complete -c dnf -n "__fish_seen_subcommand_from clean" -xa packages -d "Removes any cached packages"
complete -c dnf -n "__fish_seen_subcommand_from clean" -xa all -d "Removes all cache"

# Distro-sync
complete -c dnf -n __fish_use_subcommand -xa distro-sync -d "Synchronizes packages to match the latest"

# Downgrade
complete -c dnf -n __fish_use_subcommand -xa downgrade -d "Downgrades the specified package"
complete -c dnf -n "__fish_seen_subcommand_from downgrade" -xa "(__dnf_list_installed_packages)"

# Group
complete -c dnf -n __fish_use_subcommand -xa group -d "Manage groups"

complete -c dnf -n "__fish_seen_subcommand_from group; and not __fish_seen_subcommand_from mark" -xa summary -d "Display overview of installed and available groups"
complete -c dnf -n "__fish_seen_subcommand_from group; and not __fish_seen_subcommand_from mark" -xa info -d "Display package list of a group"
# Group install
complete -c dnf -n "__fish_seen_subcommand_from group; and not __fish_seen_subcommand_from mark" -xa install -d "Install group"
complete -c dnf -n "__fish_seen_subcommand_from group; and __fish_seen_subcommand_from install" -l with-optional -d "Include optional packages"
# Group list
complete -c dnf -n "__fish_seen_subcommand_from group; and not __fish_seen_subcommand_from mark" -xa list -d "List groups"
complete -c dnf -n "__fish_seen_subcommand_from group; and __fish_seen_subcommand_from list" -l installed -d "List installed groups"
complete -c dnf -n "__fish_seen_subcommand_from group; and __fish_seen_subcommand_from list" -l available -d "List available groups"
complete -c dnf -n "__fish_seen_subcommand_from group; and __fish_seen_subcommand_from list" -l hidden -d "List hidden groups"
# Group remove
complete -c dnf -n "__fish_seen_subcommand_from group; and not __fish_seen_subcommand_from mark" -xa remove -d "Remove group"
complete -c dnf -n "__fish_seen_subcommand_from group; and __fish_seen_subcommand_from remove" -l with-optional -d "Include optional packages"

complete -c dnf -n "__fish_seen_subcommand_from group; and not __fish_seen_subcommand_from mark" -xa upgrade -d "Upgrade group"
# Group mark
complete -c dnf -n "__fish_seen_subcommand_from group; and not __fish_seen_subcommand_from mark" -xa mark -d "Marks group without manipulating packages"
complete -c dnf -n "__fish_seen_subcommand_from group; and __fish_seen_subcommand_from mark" -xa install -d "Mark group installed without installing packages"
complete -c dnf -n "__fish_seen_subcommand_from group; and __fish_seen_subcommand_from mark" -xa remove -d "Mark group removed without removing packages"

# Help
complete -c dnf -n __fish_use_subcommand -xa help -d "Display help and exit"

# History
complete -c dnf -n __fish_use_subcommand -xa history -d "View and manage past transactions"
complete -c dnf -n "__fish_seen_subcommand_from history" -xa list -d "Lists all transactions"
complete -c dnf -n "__fish_seen_subcommand_from history" -xa info -d "Describe the given transactions"
complete -c dnf -n "__fish_seen_subcommand_from history" -xa redo -d "Redoes the specified transaction"
complete -c dnf -n "__fish_seen_subcommand_from history" -xa rollback -d "Undo all transactions performed after the specified transaction"
complete -c dnf -n "__fish_seen_subcommand_from history" -xa undo -d "Undoes the specified transaction"
complete -c dnf -n "__fish_seen_subcommand_from history" -xa userinstalled -d "Lists all user installed packages"

for i in info redo rollback undo
    complete -c dnf -n "__fish_seen_subcommand_from history; and  __fish_seen_subcommand_from $i" -xa "(__dnf_list_transactions)"
end

# Info
complete -c dnf -n __fish_use_subcommand -xa info -d "Describes the given package"
complete -c dnf -n "__fish_seen_subcommand_from info; and not __fish_seen_subcommand_from history" -k -xa "(__dnf_list_available_packages)"

# Install
complete -c dnf -n __fish_use_subcommand -xa install -d "Install package"
complete -c dnf -n "__fish_seen_subcommand_from install" -k -xa "(__dnf_list_available_packages)"

# List
complete -c dnf -n __fish_use_subcommand -xa list -d "Lists all packages"
complete -c dnf -n "__fish_seen_subcommand_from list" -l all -d "Lists all packages"
complete -c dnf -n "__fish_seen_subcommand_from list" -l installed -d "Lists installed packages"
complete -c dnf -n "__fish_seen_subcommand_from list" -l available -d "Lists available packages"
complete -c dnf -n "__fish_seen_subcommand_from list" -l extras -d "Lists installed packages that are not in any known repository"
complete -c dnf -n "__fish_seen_subcommand_from list" -l obsoletes -d "List installed obsoleted packages"
complete -c dnf -n "__fish_seen_subcommand_from list" -l recent -d "List recently added packages"
complete -c dnf -n "__fish_seen_subcommand_from list" -l upgrades -d "List available upgrades"
complete -c dnf -n "__fish_seen_subcommand_from list" -l autoremove -d "List packages which will be removed by autoremove"

# Makecache
complete -c dnf -n __fish_use_subcommand -xa makecache -d "Downloads and caches metadata for all known repos"
complete -c dnf -n "__fish_seen_subcommand_from makecache" -l timer -d "Instructs DNF to be more resource-aware"

# Mark
complete -c dnf -n __fish_use_subcommand -xa mark -d "Mark packages"
complete -c dnf -n "__fish_seen_subcommand_from mark" -xa install -d "Mark package installed"
complete -c dnf -n "__fish_seen_subcommand_from mark" -xa remove -d "Unmarks installed package"
complete -c dnf -n "__fish_seen_subcommand_from mark" -xa group -d "Mark installed by group"

# Module
complete -c dnf -n __fish_use_subcommand -xa module -d "Manage modules"
complete -c dnf -n "__fish_seen_subcommand_from module" -xa install -d "Install module"
complete -c dnf -n "__fish_seen_subcommand_from module" -xa update -d "Update modules"
complete -c dnf -n "__fish_seen_subcommand_from module" -xa remove -d "Remove module"
complete -c dnf -n "__fish_seen_subcommand_from module" -xa enable -d "Enable a module"
complete -c dnf -n "__fish_seen_subcommand_from module" -xa disable -d "Disable a module"
complete -c dnf -n "__fish_seen_subcommand_from module" -xa reset -d "Reset module state"
complete -c dnf -n "__fish_seen_subcommand_from module" -xa list -d "List modules"
complete -c dnf -n "__fish_seen_subcommand_from module; and __fish_seen_subcommand_from list" -l all -d "Lists all module "
complete -c dnf -n "__fish_seen_subcommand_from module; and __fish_seen_subcommand_from list" -l enabled -d "Lists enabled module"
complete -c dnf -n "__fish_seen_subcommand_from module; and __fish_seen_subcommand_from list" -l disabled -d "Lists disabled module"
complete -c dnf -n "__fish_seen_subcommand_from module; and __fish_seen_subcommand_from list" -l installed -d "List  installed modules"
complete -c dnf -n "__fish_seen_subcommand_from module" -xa info -d "Print module information"
complete -c dnf -n "__fish_seen_subcommand_from module; and __fish_seen_subcommand_from info" -l profile -d "Print module profiles information"

# Offline-distrosync
complete -c dnf -n __fish_use_subcommand -xa offline-distrosync -d "Synchronizes packages to match the latest"
complete -c dnf -n "__fish_seen_subcommand_from offline-distrosync" -xa download -d "Download updates for offline upgrade"
complete -c dnf -n "__fish_seen_subcommand_from offline-distrosync" -xa clean -d "Remove cached packages"
complete -c dnf -n "__fish_seen_subcommand_from offline-distrosync" -xa reboot -d "Reboot and install packages"
complete -c dnf -n "__fish_seen_subcommand_from offline-distrosync" -xa upgrade -d "Install cached packages without reboot"
complete -c dnf -n "__fish_seen_subcommand_from offline-distrosync" -xa log -d "Show logs of upgrade attempts"

# Offline-upgrade
complete -c dnf -n __fish_use_subcommand -xa offline-upgrade -d "Prepare offline upgrade of the system"
complete -c dnf -n "__fish_seen_subcommand_from offline-upgrade" -xa download -d "Download updates for offline upgrade"
complete -c dnf -n "__fish_seen_subcommand_from offline-upgrade" -xa clean -d "Remove cached packages"
complete -c dnf -n "__fish_seen_subcommand_from offline-upgrade" -xa reboot -d "Reboot and install packages"
complete -c dnf -n "__fish_seen_subcommand_from offline-upgrade" -xa log -d "Show logs of upgrade attempts"

# Provides
complete -c dnf -n __fish_use_subcommand -xa provides -d "Finds packages providing the given command"

# Reinstall
complete -c dnf -n __fish_use_subcommand -xa reinstall -d "Reinstalls a package"
complete -c dnf -n "__fish_seen_subcommand_from reinstall" -xa "(__dnf_list_installed_packages)"

# Remove
complete -c dnf -n __fish_use_subcommand -xa remove -d "Remove packages"
complete -c dnf -n "__fish_seen_subcommand_from remove" -xa "(__dnf_list_installed_packages)" -d "Removes the specified packages"
complete -c dnf -n "__fish_seen_subcommand_from remove" -l duplicates -d "Removes older version of duplicated packages"
complete -c dnf -n "__fish_seen_subcommand_from remove" -l oldinstallonly -d "Removes old installonly packages"

# Repolist and Repoinfo
complete -c dnf -n __fish_use_subcommand -xa repoinfo -d "Verbose repolist"
complete -c dnf -n __fish_use_subcommand -xa repolist -d "Lists all enabled repositories"

for i in repolist repoinfo
    complete -c dnf -n "__fish_seen_subcommand_from $i" -l enabled -d "Lists all enabled repositories"
    complete -c dnf -n "__fish_seen_subcommand_from $i" -l disabled -d "Lists all disabled repositories"
    complete -c dnf -n "__fish_seen_subcommand_from $i" -l all -d "Lists all repositories"
end

# Repoquery
complete -c dnf -n __fish_use_subcommand -xa repoquery -d "Queries DNF repositories"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l querytags -d "Provides the list of tags"

# repoquery select options
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -s a -l all
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l enabled
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l arch -l archlist
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l duplicates
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l unneeded
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l available
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l extras
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -s f -l file
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l installed
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l installonly
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l latest-limit
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l recent
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l repo
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l unsatisfied
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l upgrades
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l userinstalled
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l whatdepends
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l whatconflicts
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l whatenhances
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l whatobsoletes
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l whatprovides
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l whatrecommends
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l whatrequires
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l whatsuggests
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l whatsupplements
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l alldeps
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l exactdeps
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l srpm

# System-upgrade
complete -c dnf -n __fish_use_subcommand -xa system-upgrade -d "Prepare major version upgrade of the system"
complete -c dnf -n "__fish_seen_subcommand_from system-upgrade" -xa download -d "Download updates for offline upgrade"
complete -c dnf -n "__fish_seen_subcommand_from system-upgrade" -xa clean -d "Remove cached packages"
complete -c dnf -n "__fish_seen_subcommand_from system-upgrade" -xa reboot -d "Reboot and install packages"
complete -c dnf -n "__fish_seen_subcommand_from system-upgrade" -xa upgrade -d "Install cached packages without reboot"
complete -c dnf -n "__fish_seen_subcommand_from system-upgrade" -xa log -d "Show logs of upgrade attempts"

# Query options
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -s i -l info -d "Show detailed information about the package"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -s l -l list -d "Show the list of files in the package"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -s s -l source -d "Show the package source RPM name"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l changelogs -d "Print the package changelogs"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l conflicts -d "Display capabilities that the package conflicts with"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l depends -d "Display capabilities that the package depends on"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l enhances -d "Display capabilities enhanced by the package"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l location -d "Show a location where the package could be downloaded from"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l obsoletes -d "Display capabilities that the package obsoletes"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l provides -d "Display capabilities provided by the package"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l recommends -d "Display capabilities recommended by the package"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l requires -d "Display capabilities that the package depends on"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l requires-pre -d "Display capabilities that the package depends on"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l suggests -d "Display capabilities suggested by the package"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l supplements -d "Display capabilities supplemented by the package"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l tree -d "Display a recursive tree of packages"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l deplist -d "Produce a list of all dependencies"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l nvr -d "Format like name-version-release"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l nevra -d "Format like name-epoch:version-release.architecture"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l envra -d "Format like epoch:name-version-release.architecture"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l qf -l queryformat -d "Custom display format"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l recursive -d "Query packages recursively"
complete -c dnf -n "__fish_seen_subcommand_from repoquery" -l resolve -d "Resolve capabilities to originating packages"

# Repository-Packages
complete -c dnf -n __fish_use_subcommand -xa repository-packages -d "Run commands on all packages in the repository"

# Search
complete -c dnf -n __fish_use_subcommand -xa search -d "Search package metadata for keywords"
complete -c dnf -n "__fish_seen_subcommand_from search" -l all -d "Lists packages that match at least one of the keys"

# Shell
complete -c dnf -n __fish_use_subcommand -xa shell -d "Opens an interactive shell"

# Swap
complete -c dnf -n __fish_use_subcommand -xa swap -d "Remove spec and install spec in one transaction"

# Updateinfo
complete -c dnf -n __fish_use_subcommand -xa updateinfo -d "Display information about update advisories"
complete -c dnf -n "__fish_seen_subcommand_from updateinfo" -l summary -d "Displays the summary"
complete -c dnf -n "__fish_seen_subcommand_from updateinfo" -l list -d "List of advisories"
complete -c dnf -n "__fish_seen_subcommand_from updateinfo" -l info -d "Detailed information"

# updateinfo <availability> options
complete -c dnf -n "__fish_seen_subcommand_from updateinfo" -l all
complete -c dnf -n "__fish_seen_subcommand_from updateinfo" -l available
complete -c dnf -n "__fish_seen_subcommand_from updateinfo" -l installed
complete -c dnf -n "__fish_seen_subcommand_from updateinfo" -l updates

# Upgrade
complete -c dnf -n __fish_use_subcommand -xa upgrade -d "Updates packages"
complete -c dnf -n "__fish_seen_subcommand_from upgrade" -xa "(__dnf_list_installed_packages)"

# Upgrade-Minimal
complete -c dnf -n __fish_use_subcommand -xa upgrade-minimal -d "Updates packages"
complete -c dnf -n "__fish_seen_subcommand_from upgrade-minimal" -xa "(__dnf_list_installed_packages)"

# Versionlock
if test -f /etc/dnf/plugins/versionlock.conf
    function __dnf_current_versionlock_list
        dnf versionlock list | grep -v metadata
    end

    complete -c dnf -n __fish_use_subcommand -xa versionlock -d "DNF versionlock plugin"
    # - add
    complete -c dnf -n "__fish_seen_subcommand_from versionlock" -xa add -d "Add  a versionlock for all available packages matching the spec"
    complete -c dnf -n "__fish_seen_subcommand_from versionlock; and  __fish_seen_subcommand_from add" -xa "(__dnf_list_installed_packages)"
    # - exclude
    complete -c dnf -n "__fish_seen_subcommand_from versionlock" -xa exclude -d "Add an exclude (within  versionlock) for the available packages matching the spec"
    complete -c dnf -n "__fish_seen_subcommand_from versionlock; and  __fish_seen_subcommand_from exclude" -xa "(__dnf_list_installed_packages)"
    # - delete
    complete -c dnf -n "__fish_seen_subcommand_from versionlock" -xa delete -d "Remove any matching versionlock entries"
    complete -c dnf -n "__fish_seen_subcommand_from versionlock; and  __fish_seen_subcommand_from delete" -xa "(__dnf_current_versionlock_list)"
    # - list
    complete -c dnf -n "__fish_seen_subcommand_from versionlock" -xa list -d "List the current versionlock entries"
    complete -c dnf -n "__fish_seen_subcommand_from versionlock; and  __fish_seen_subcommand_from list" -xa "(false)"
    # - clear
    complete -c dnf -n "__fish_seen_subcommand_from versionlock" -xa clear -d "Remove all versionlock entries"
    complete -c dnf -n "__fish_seen_subcommand_from versionlock; and  __fish_seen_subcommand_from clear" -xa "(false)"
end

# Options:
# Using __fish_no_arguments here so that users are not completely overloaded with
#   available options when using subcommands (e.g. repoquery) (40 vs 100ish)
complete -c dnf -n __fish_no_arguments -s 4 -d "Use IPv4 only"
complete -c dnf -n __fish_no_arguments -s 6 -d "Use IPv6 only"
complete -c dnf -n __fish_no_arguments -l advisory -l advisories -d "Include packages corresponding to the advisory ID"
complete -c dnf -n __fish_no_arguments -l allowerasing -d "Allow erasing of installed packages to resolve dependencies"
complete -c dnf -n __fish_no_arguments -l assumeno -d "Answer no for all questions"
complete -c dnf -n __fish_no_arguments -s b -l best -d "Try the best available package versions in transactions"
complete -c dnf -n __fish_no_arguments -l bugfix -d "Include packages that fix a bugfix issue"
complete -c dnf -n __fish_no_arguments -l bz -l bzs -d "Include packages that fix a Bugzilla ID"
complete -c dnf -n __fish_no_arguments -s C -l cacheonly -d "Run entirely from system cache"
complete -c dnf -n __fish_no_arguments -l color -xa "always never auto" -d "Control whether color is used"
complete -c dnf -n __fish_no_arguments -s c -l config -d "Configuration file location"
complete -c dnf -n __fish_no_arguments -l cve -l cves -d "Include packages that fix a CVE"
complete -c dnf -n __fish_no_arguments -s d -l debuglevel -d "Debugging output level"
complete -c dnf -n __fish_no_arguments -l debugsolver -d "Dump dependency solver debugging info"
complete -c dnf -n __fish_no_arguments -l disableexcludes -l disableexcludepkgs -d "Disable excludes"
complete -c dnf -n __fish_no_arguments -l disable -l set-disabled -d "Disable specified repositories"
complete -c dnf -n __fish_no_arguments -l disableplugin -d "Disable the listed plugins specified"
complete -c dnf -n __fish_no_arguments -l disablerepo -d "Disable specified repositories"
complete -c dnf -n __fish_no_arguments -l downloaddir -l destdir -d "Change downloaded packages to provided directory"
complete -c dnf -n __fish_no_arguments -l downloadonly -d "Download packages without performing any transaction"
complete -c dnf -n __fish_no_arguments -l enable -l set-enabled -d "Enable specified repositories"
complete -c dnf -n __fish_no_arguments -l enableplugin -d "Enable the listed plugins"
complete -c dnf -n __fish_no_arguments -l enablerepo -d "Enable additional repositories"
complete -c dnf -n __fish_no_arguments -l enhancement -d "Include enhancement relevant packages"
complete -c dnf -n __fish_no_arguments -s x -l exclude -d "Exclude packages specified"
complete -c dnf -n __fish_no_arguments -l forcearch -d "Force the use of the specified architecture"
complete -c dnf -n __fish_no_arguments -s h -l help -l help-i -d "Show the help"
complete -c dnf -n __fish_no_arguments -l installroot -d "Specifies an alternative installroot"
complete -c dnf -n __fish_no_arguments -l newpackage -d "Include newpackage relevant packages"
complete -c dnf -n __fish_no_arguments -l noautoremove -d "Disable autoremove"
complete -c dnf -n __fish_no_arguments -l nobest -d "Set best option to False"
complete -c dnf -n __fish_no_arguments -l nodocs -d "Do not install documentation"
complete -c dnf -n __fish_no_arguments -l nogpgcheck -d "Skip checking GPG signatures on packages"
complete -c dnf -n __fish_no_arguments -l noplugins -d "Disable all plugins"
complete -c dnf -n __fish_no_arguments -l obsoletes -d "Enables obsoletes processing logic"
complete -c dnf -n __fish_no_arguments -s q -l quiet -d "Quiet mode"
complete -c dnf -n __fish_no_arguments -s R -l randomwait -d "Maximum command wait time"
complete -c dnf -n __fish_no_arguments -l refresh -d "Set metadata as expired before running the command"
complete -c dnf -n __fish_no_arguments -l releasever -d "Configure the distribution release"
complete -c dnf -n __fish_no_arguments -l repofrompath -d "Specify repository to add to the repositories for this query"
complete -c dnf -n __fish_no_arguments -l repo -l repoid -d "Enable just specific repositories by an id or a glob"
complete -c dnf -n __fish_no_arguments -l rpmverbosity -d "RPM debug scriptlet output level"
complete -c dnf -n __fish_no_arguments -l sec-severity -l secseverity -d "Includes packages that provide a fix for an issue of the specified severity"
complete -c dnf -n __fish_no_arguments -l security -d "Includes packages that provide a fix for a security issue"
complete -c dnf -n __fish_no_arguments -l setopt -d "Override a configuration option"
complete -c dnf -n __fish_no_arguments -l skip-broken -d "Skips broken packages"
complete -c dnf -n __fish_no_arguments -l showduplicates -d "Shows duplicate packages"
complete -c dnf -n __fish_no_arguments -s v -l verbose -d "Verbose mode"
complete -c dnf -n __fish_no_arguments -l version -d "Shows DNF version and exit"
complete -c dnf -n __fish_no_arguments -s y -l assumeyes -d "Answer yes for all questions"
