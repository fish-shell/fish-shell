function __fish_composer_needs_command
    set -l cmd (commandline -xpc)

    if test (count $cmd) -eq 1
        return 0
    end

    return 1
end

function __fish_composer_using_command
    set -l cmd (commandline -xpc)

    if test (count $cmd) -gt 1
        if test $argv[1] = $cmd[2]
            return 0
        end
    end

    return 1
end

function __fish_composer_required_packages
    test -f composer.json; or return
    set -l python (__fish_anypython); or return
    echo "
import itertools
import json
json_data = open('composer.json')
data = json.load(json_data)
json_data.close()
packages = itertools.chain(data.get('require', {}).keys(), data.get('require-dev', {}).keys())
print(\"\n\".join(packages))
      " | $python -S
end

function __fish_composer_installed_packages
    test -f composer.lock; or return
    set -l python (__fish_anypython); or return
    echo "
import json
json_data = open('composer.lock')
data = json.load(json_data)
json_data.close()
installed_packages = []
for package in data['packages']:
    installed_packages.append(package['name'])
for package in data['packages-dev']:
    installed_packages.append(package['name'])
print(\"\n\".join(installed_packages))
" | $python -S
end

function __fish_composer_scripts
    test -f composer.json; or return
    set -l python (__fish_anypython); or return
    echo "
import json
json_data = open('composer.json')
data = json.load(json_data)
json_data.close()
if 'scripts' in data and data['scripts']:
    print(\"\n\".join(data['scripts'].keys()))
" | $python -S
end

# add cmds list
set --local composer_cmds about archive browse check-platform-reqs clear-cache config create-project depends diagnose dump-autoload exec global help home init install licenses list outdated prohibits remove require run-script search self-update show suggests status update validate why why-not

# Custom scripts
complete -f -c composer -n __fish_composer_needs_command -a '(__fish_composer_scripts)' -d 'User script'
complete -f -c composer -n '__fish_composer_using_command run-script' -a "(__fish_composer_scripts)"
complete -f -c composer -n '__fish_composer_using_command run-script' -l timeout -d 'Sets script timeout in seconds, or 0 for never'
complete -f -c composer -n '__fish_composer_using_command run-script' -l dev -d 'Sets the dev mode'
complete -f -c composer -n '__fish_composer_using_command run-script' -l no-dev -d 'Disables the dev mode'
complete -f -c composer -n '__fish_composer_using_command run-script' -l list -d 'List scripts'

# commands
complete -f -c composer -n __fish_composer_needs_command -a about -d 'Short information about Composer'
complete -f -c composer -n __fish_composer_needs_command -a archive -d 'Create an archive of this composer package'
complete -f -c composer -n __fish_composer_needs_command -a browse -d 'Opens the package\'s repository URL or homepage in your browser'
complete -f -c composer -n __fish_composer_needs_command -a check-platform-reqs -d 'Check that platform requirements are satisfied'
complete -f -c composer -n __fish_composer_needs_command -a clear-cache -d 'Clears composer\'s internal package cache'
complete -f -c composer -n __fish_composer_needs_command -a config -d 'Set config options'
complete -f -c composer -n __fish_composer_needs_command -a create-project -d 'Create new project from a package into given directory'
complete -f -c composer -n __fish_composer_needs_command -a depends -d 'Shows which packages depend on the given package'
complete -f -c composer -n __fish_composer_needs_command -a diagnose -d 'Diagnoses the system to identify common errors'
complete -f -c composer -n __fish_composer_needs_command -a dump-autoload -d 'Dumps the autoloader'
complete -f -c composer -n __fish_composer_needs_command -a exec -d 'Executes a vendored binary/script'
complete -f -c composer -n __fish_composer_needs_command -a global -d 'Allows running commands in the global composer dir ($COMPOSER_HOME)'
complete -f -c composer -n __fish_composer_needs_command -a help -d 'Displays help for a command'
complete -f -c composer -n __fish_composer_needs_command -a home -d 'Opens the package\'s repository URL or homepage in your browser'
complete -f -c composer -n __fish_composer_needs_command -a init -d 'Creates a basic composer.json file in current directory'
complete -f -c composer -n __fish_composer_needs_command -a install -d 'Install dependencies from composer.lock or composer.json'
complete -f -c composer -n __fish_composer_needs_command -a licenses -d 'Show information about licenses of dependencies'
complete -f -c composer -n __fish_composer_needs_command -a list -d 'Lists commands'
complete -f -c composer -n __fish_composer_needs_command -a outdated -d 'Shows a list of installed packages with available updates'
complete -f -c composer -n __fish_composer_needs_command -a prohibits -d 'Shows which packages prevent the given package from being installed'
complete -f -c composer -n __fish_composer_needs_command -a remove -d 'Removes a package from the require or require-dev'
complete -f -c composer -n __fish_composer_needs_command -a require -d 'Adds required packages to your composer.json and installs them'
complete -f -c composer -n __fish_composer_needs_command -a run-script -d 'Run the scripts defined in composer.json'
complete -f -c composer -n __fish_composer_needs_command -a search -d 'Search for packages'
complete -f -c composer -n __fish_composer_needs_command -a self-update -d 'Updates composer.phar to the latest version'
complete -f -c composer -n __fish_composer_needs_command -a show -d 'Show information about packages'
complete -f -c composer -n __fish_composer_needs_command -a status -d 'Show a list of locally modified packages'
complete -f -c composer -n __fish_composer_needs_command -a suggests -d 'Shows package suggestions'
complete -f -c composer -n __fish_composer_needs_command -a update -d 'Update dependencies according to composer.json, and update composer.lock'
complete -f -c composer -n __fish_composer_needs_command -a validate -d 'Validates a composer.json'
complete -f -c composer -n __fish_composer_needs_command -a why -d 'Shows which packages cause the given package to be installed'
complete -f -c composer -n __fish_composer_needs_command -a why-not -d 'Shows which packages prevent the given package from being installed'

# archive
complete -f -c composer -n '__fish_composer_using_command archive' -l format -d 'Format of the resulting archive: tar or zip'
complete -f -c composer -n '__fish_composer_using_command archive' -l dir -d 'Write the archive to this directory'
complete -f -c composer -n '__fish_composer_using_command archive' -l file -d 'Write the archive with the given file name. Note that the format will be appended'
complete -f -c composer -n '__fish_composer_using_command archive' -l ignore-filters -d 'Ignore filters when saving package'

# browse
complete -f -c composer -n '__fish_composer_using_command browse' -l homepage -d 'Open the homepage instead of the repository URL'
complete -f -c composer -n '__fish_composer_using_command browse' -l show -d 'Only show the homepage or repository URL'

# check-platform-reqs
complete -f -c composer -n '__fish_composer_using_command check-platform-reqs' -l no-dev -d 'Disables checking of require-dev packages requirements'

# config
complete -f -c composer -n '__fish_composer_using_command config' -l global -d 'Apply command to the global config file'
complete -f -c composer -n '__fish_composer_using_command config' -l editor -d 'Open editor'
complete -f -c composer -n '__fish_composer_using_command config' -l auth -d 'Affect auth config file (only used for --editor)'
complete -f -c composer -n '__fish_composer_using_command config' -l unset -d 'Unset the given setting-key'
complete -f -c composer -n '__fish_composer_using_command config' -l list -d 'List configuration settings'
complete -f -c composer -n '__fish_composer_using_command config' -l file -d 'If you want to choose a different composer.json or config.json'
complete -f -c composer -n '__fish_composer_using_command config' -l absolute -d 'Returns absolute paths when fetching *-dir config values instead of relative'

# create-project
complete -f -c composer -n '__fish_composer_using_command create-project' -l stability -d 'Minimum-stability allowed (unless a version is specified)'
complete -f -c composer -n '__fish_composer_using_command create-project' -l prefer-source -d 'Forces installation from package sources when possible, including VCS information'
complete -f -c composer -n '__fish_composer_using_command create-project' -l prefer-dist -d 'Forces installation from package dist even for dev versions'
complete -f -c composer -n '__fish_composer_using_command create-project' -l repository -d 'Pick a different repository (as url or json config) to look for the package'
complete -f -c composer -n '__fish_composer_using_command create-project' -l repository-url -d 'DEPRECATED: Use --repository instead'
complete -f -c composer -n '__fish_composer_using_command create-project' -l dev -d 'Enables installation of require-dev packages (enabled by default, only present for BC)'
complete -f -c composer -n '__fish_composer_using_command create-project' -l no-dev -d 'Disables installation of require-dev packages'
complete -f -c composer -n '__fish_composer_using_command create-project' -l no-custom-installers -d 'DEPRECATED: Use no-plugins instead'
complete -f -c composer -n '__fish_composer_using_command create-project' -l no-scripts -d 'Whether to prevent execution of all defined scripts in the root package'
complete -f -c composer -n '__fish_composer_using_command create-project' -l no-progress -d 'Do not output download progress'
complete -f -c composer -n '__fish_composer_using_command create-project' -l no-secure-http -d 'Disable secure-http while installing the root package. (This is a BAD IDEA)'
complete -f -c composer -n '__fish_composer_using_command create-project' -l keep-vcs -d 'Whether to prevent deleting the vcs folder'
complete -f -c composer -n '__fish_composer_using_command create-project' -l remove-vcs -d 'Whether to force deletion of the vcs folder without prompting'
complete -f -c composer -n '__fish_composer_using_command create-project' -l no-install -d 'Whether to skip installation of the package dependencies'
complete -f -c composer -n '__fish_composer_using_command create-project' -l ignore-platform-reqs -d 'Ignore platform requirements (php & ext- packages)'

# depends
complete -f -c composer -n '__fish_composer_using_command depends' -a "(__fish_composer_installed_packages)"
complete -f -c composer -n '__fish_composer_using_command depends' -l recursive -d 'Recursively resolves up to the root package'
complete -f -c composer -n '__fish_composer_using_command depends' -l tree -d 'Prints the results as a nested tree'

# dump-autoload
complete -f -c composer -n '__fish_composer_using_command dump-autoload' -l no-scripts -d 'Skips the execution of all scripts defined in composer.json file'
complete -f -c composer -n '__fish_composer_using_command dump-autoload' -l optimize -d 'Optimizes PSR0 and PSR4 packages to be loaded with classmaps too, good for production'
complete -f -c composer -n '__fish_composer_using_command dump-autoload' -l classmap-authoritative -d 'Autoload classes from the classmap only. Implicitly enables `--optimize`'
complete -f -c composer -n '__fish_composer_using_command dump-autoload' -l apcu -d 'Use APCu to cache found/not-found classes'
complete -f -c composer -n '__fish_composer_using_command dump-autoload' -l no-dev -d 'Disables autoload-dev rules'

# exec
complete -f -c composer -n '__fish_composer_using_command exec' -l list

# help
complete -f -c composer -n '__fish_composer_using_command help' -a "$composer_cmds"
complete -f -c composer -n '__fish_composer_using_command help' -l xml -d 'To output help as XML'
complete -f -c composer -n '__fish_composer_using_command help' -l format -d 'The output format (txt, xml, json, or md)'
complete -f -c composer -n '__fish_composer_using_command help' -l raw -d 'To output raw command help'

# init
complete -f -c composer -n '__fish_composer_using_command init' -l name -d 'Name of the package'
complete -f -c composer -n '__fish_composer_using_command init' -l description -d 'Description of package'
complete -f -c composer -n '__fish_composer_using_command init' -l author -d 'Author name of package'
complete -f -c composer -n '__fish_composer_using_command init' -l type -d 'Type of package (e.g. library, project, metapackage, composer-plugin)'
complete -f -c composer -n '__fish_composer_using_command init' -l homepage -d 'Homepage of package'
complete -f -c composer -n '__fish_composer_using_command init' -l require -d 'Package to require with a version constraint, e.g. foo/bar:1.0.0 or foo/bar=1.0.0 or "foo/bar 1.0.0"'
complete -f -c composer -n '__fish_composer_using_command init' -l require-dev -d 'Package to require for development with a version constraint, e.g. foo/bar:1.0.0 or foo/bar=1.0.0 or "foo/bar 1.0.0"'
complete -f -c composer -n '__fish_composer_using_command init' -l stability -d 'Minimum stability (empty or one of: stable, RC, beta, alpha, dev)'
complete -f -c composer -n '__fish_composer_using_command init' -l license -d 'License of package'
complete -f -c composer -n '__fish_composer_using_command init' -l repository -d 'Add custom repositories, either by URL or using JSON arrays'

# install
complete -f -c composer -n '__fish_composer_using_command install' -l prefer-source -d 'Forces installation from package sources when possible, including VCS information'
complete -f -c composer -n '__fish_composer_using_command install' -l prefer-dist -d 'Forces installation from package dist even for dev versions'
complete -f -c composer -n '__fish_composer_using_command install' -l dry-run -d 'Outputs the operations but will not execute anything (implicitly enables --verbose)'
complete -f -c composer -n '__fish_composer_using_command install' -l dev -d 'Enables installation of require-dev packages (enabled by default, only present for BC)'
complete -f -c composer -n '__fish_composer_using_command install' -l no-dev -d 'Disables installation of require-dev packages'
complete -f -c composer -n '__fish_composer_using_command install' -l no-custom-installers -d 'DEPRECATED: Use no-plugins instead'
complete -f -c composer -n '__fish_composer_using_command install' -l no-autoloader -d 'Skips autoloader generation'
complete -f -c composer -n '__fish_composer_using_command install' -l no-scripts -d 'Skips the execution of all scripts defined in composer.json file'
complete -f -c composer -n '__fish_composer_using_command install' -l no-progress -d 'Do not output download progress'
complete -f -c composer -n '__fish_composer_using_command install' -l no-suggest -d 'Do not show package suggestions'
complete -f -c composer -n '__fish_composer_using_command install' -l optimize-autoloader -d 'Optimize autoloader during autoloader dump'
complete -f -c composer -n '__fish_composer_using_command install' -l classmap-authoritative -d 'Autoload classes from the classmap only. Implicitly enables `--optimize-autoloader`'
complete -f -c composer -n '__fish_composer_using_command install' -l apcu-autoloader -d 'Use APCu to cache found/not-found classes'
complete -f -c composer -n '__fish_composer_using_command install' -l ignore-platform-reqs -d 'Ignore platform requirements (php & ext- packages)'

# licenses
complete -f -c composer -n '__fish_composer_using_command licenses' -l format -d 'Format of the output: text or json'
complete -f -c composer -n '__fish_composer_using_command licenses' -l no-dev -d 'Disables search in require-dev packages'

# list
complete -f -c composer -n '__fish_composer_using_command list' -l xml -d 'To output list as XML'
complete -f -c composer -n '__fish_composer_using_command list' -l raw -d 'To output raw command list'
complete -f -c composer -n '__fish_composer_using_command list' -l format -d 'The output format (txt, xml, json, or md)'

# outdated
complete -f -c composer -n '__fish_composer_using_command outdated' -l outdated -d 'Show only packages that are outdated'
complete -f -c composer -n '__fish_composer_using_command outdated' -l all -d 'Show all installed packages with their latest versions'
complete -f -c composer -n '__fish_composer_using_command outdated' -l direct -d 'Shows only packages that are directly required by the root package'
complete -f -c composer -n '__fish_composer_using_command outdated' -l strict -d 'Return a non-zero exit code when there are outdated packages'
complete -f -c composer -n '__fish_composer_using_command outdated' -l minor-only -d 'Show only packages that have minor SemVer-compatible updates. Use with the --outdated option'
complete -f -c composer -n '__fish_composer_using_command outdated' -l format -d 'Format of the output: text or json'
complete -f -c composer -n '__fish_composer_using_command outdated' -l ignore -d 'Ignore specified package(s). Use it with the --outdated option if you don\'t want to be informed about new versions of some packages'

# prohibits
complete -f -c composer -n '__fish_composer_using_command prohibits' -l recursive -d 'Recursively resolves up to the root package'
complete -f -c composer -n '__fish_composer_using_command prohibits' -l tree -d 'Prints the results as a nested tree'

# remove
complete -f -c composer -n '__fish_composer_using_command remove' -a "(__fish_composer_required_packages)"
complete -f -c composer -n '__fish_composer_using_command remove' -l dev -d 'Removes a package from the require-dev section'
complete -f -c composer -n '__fish_composer_using_command remove' -l no-progress -d 'Do not output download progress'
complete -f -c composer -n '__fish_composer_using_command remove' -l no-update -d 'Disables the automatic update of the dependencies'
complete -f -c composer -n '__fish_composer_using_command remove' -l no-scripts -d 'Skips the execution of all scripts defined in composer.json file'
complete -f -c composer -n '__fish_composer_using_command remove' -l update-no-dev -d 'Run the dependency update with the --no-dev option'
complete -f -c composer -n '__fish_composer_using_command remove' -l update-with-dependencies -d 'Allow inherited dependencies to be updated with explicit dependencies (default)'
complete -f -c composer -n '__fish_composer_using_command remove' -l no-update-with-dependencies -d 'Disallow inherited dependencies to be updated with explicit dependencies'
complete -f -c composer -n '__fish_composer_using_command remove' -l ignore-platform-reqs -d 'Ignore platform requirements (php & ext- packages)'
complete -f -c composer -n '__fish_composer_using_command remove' -l optimize-autoloader -d 'Optimize autoloader during autoloader dump'
complete -f -c composer -n '__fish_composer_using_command remove' -l classmap-authoritative -d 'Autoload classes from the classmap only. Implicitly enables `--optimize-autoloader`'
complete -f -c composer -n '__fish_composer_using_command remove' -l apcu-autoloader -d 'Use APCu to cache found/not-found classes'

# require
complete -f -c composer -n '__fish_composer_using_command require' -l dev -d 'Add requirement to require-dev'
complete -f -c composer -n '__fish_composer_using_command require' -l prefer-source -d 'Forces installation from package sources when possible, including VCS information'
complete -f -c composer -n '__fish_composer_using_command require' -l prefer-dist -d 'Forces installation from package dist even for dev versions'
complete -f -c composer -n '__fish_composer_using_command require' -l no-progress -d 'Do not output download progress'
complete -f -c composer -n '__fish_composer_using_command require' -l no-suggest -d 'Do not show package suggestions'
complete -f -c composer -n '__fish_composer_using_command require' -l no-update -d 'Disables the automatic update of the dependencies'
complete -f -c composer -n '__fish_composer_using_command require' -l no-scripts -d 'Skips the execution of all scripts defined in composer.json file'
complete -f -c composer -n '__fish_composer_using_command require' -l update-no-dev -d 'Run the dependency update with the --no-dev option'
complete -f -c composer -n '__fish_composer_using_command require' -l update-with-dependencies -d 'Allows inherited dependencies to be updated, except those that are root requirements'
complete -f -c composer -n '__fish_composer_using_command require' -l update-with-all-dependencies -d 'Allows all inherited dependencies to be updated, including those that are root requirements'
complete -f -c composer -n '__fish_composer_using_command require' -l ignore-platform-reqs -d 'Ignore platform requirements (php & ext- packages)'
complete -f -c composer -n '__fish_composer_using_command require' -l prefer-stable -d 'Prefer stable versions of dependencies'
complete -f -c composer -n '__fish_composer_using_command require' -l prefer-lowest -d 'Prefer lowest versions of dependencies'
complete -f -c composer -n '__fish_composer_using_command require' -l sort-packages -d 'Sorts packages when adding/updating a new dependency'
complete -f -c composer -n '__fish_composer_using_command require' -l optimize-autoloader -d 'Optimize autoloader during autoloader dump'
complete -f -c composer -n '__fish_composer_using_command require' -l classmap-authoritative -d 'Autoload classes from the classmap only. Implicitly enables `--optimize-autoloader`'
complete -f -c composer -n '__fish_composer_using_command require' -l apcu-autoloader -d 'Use APCu to cache found/not-found classes'

# search
complete -f -c composer -n '__fish_composer_using_command search' -l only-name -d 'Search only in name'
complete -f -c composer -n '__fish_composer_using_command search' -l type -d 'Search for a specific package type'

# self-update
complete -f -c composer -n '__fish_composer_using_command self-update' -l rollback -d 'Revert to an older installation of composer'
complete -f -c composer -n '__fish_composer_using_command self-update' -l clean-backups -d 'Delete old backups during an update, leave only current version of composer'
complete -f -c composer -n '__fish_composer_using_command self-update' -l no-progress -d 'Do not output download progress'
complete -f -c composer -n '__fish_composer_using_command self-update' -l update-keys -d 'Prompt user for a key update'
complete -f -c composer -n '__fish_composer_using_command self-update' -l stable -d 'Force an update to the stable channel'
complete -f -c composer -n '__fish_composer_using_command self-update' -l preview -d 'Force an update to the preview channel'
complete -f -c composer -n '__fish_composer_using_command self-update' -l snapshot -d 'Force an update to the snapshot channel'
complete -f -c composer -n '__fish_composer_using_command self-update' -l set-channel-only -d 'Only store the channel as the default one and then exit'

# show
complete -f -c composer -n '__fish_composer_using_command show' -a "(__fish_composer_installed_packages)"
complete -f -c composer -n '__fish_composer_using_command show' -l all -d 'List all packages'
complete -f -c composer -n '__fish_composer_using_command show' -l installed -d 'List installed packages only (enabled by default, only present for BC)'
complete -f -c composer -n '__fish_composer_using_command show' -l platform -d 'List platform packages only'
complete -f -c composer -n '__fish_composer_using_command show' -l available -d 'List available packages only'
complete -f -c composer -n '__fish_composer_using_command show' -l self -d 'Show the root package information'
complete -f -c composer -n '__fish_composer_using_command show' -l name-only -d 'List package names only'
complete -f -c composer -n '__fish_composer_using_command show' -l path -d 'Show package paths'
complete -f -c composer -n '__fish_composer_using_command show' -l tree -d 'List the dependencies as a tree'
complete -f -c composer -n '__fish_composer_using_command show' -l latest -d 'Show the latest version'
complete -f -c composer -n '__fish_composer_using_command show' -l outdated -d 'Show the latest version but only for packages that are outdated'
complete -f -c composer -n '__fish_composer_using_command show' -l ignore -d 'Ignore specified package(s). Use it with the --outdated option if you don\'t want to be informed about new versions of some packages'
complete -f -c composer -n '__fish_composer_using_command show' -l minor-only -d 'Show only packages that have minor SemVer-compatible updates. Use with the --outdated option'
complete -f -c composer -n '__fish_composer_using_command show' -l direct -d 'Shows only packages that are directly required by the root package'
complete -f -c composer -n '__fish_composer_using_command show' -l strict -d 'Return a non-zero exit code when there are outdated packages'
complete -f -c composer -n '__fish_composer_using_command show' -l format -d 'Format of the output: text or json'

# suggests
complete -f -c composer -n '__fish_composer_using_command suggests' -l by-package -d 'Groups output by suggesting package'
complete -f -c composer -n '__fish_composer_using_command suggests' -l by-suggestion -d 'Groups output by suggested package'
complete -f -c composer -n '__fish_composer_using_command suggests' -l no-dev -d 'Exclude suggestions from require-dev packages'

# update
complete -f -c composer -n '__fish_composer_using_command update' -a "(__fish_composer_required_packages)"
complete -f -c composer -n '__fish_composer_using_command update' -l prefer-source -d 'Forces installation from package sources when possible, including VCS information'
complete -f -c composer -n '__fish_composer_using_command update' -l prefer-dist -d 'Forces installation from package dist even for dev versions'
complete -f -c composer -n '__fish_composer_using_command update' -l dry-run -d 'Outputs the operations but will not execute anything (implicitly enables --verbose)'
complete -f -c composer -n '__fish_composer_using_command update' -l dev -d 'Enables installation of require-dev packages (enabled by default, only present for BC)'
complete -f -c composer -n '__fish_composer_using_command update' -l no-dev -d 'Disables installation of require-dev packages'
complete -f -c composer -n '__fish_composer_using_command update' -l lock -d 'Only updates the lock file hash to suppress warning about the lock file being out of date'
complete -f -c composer -n '__fish_composer_using_command update' -l no-custom-installers -d 'DEPRECATED: Use no-plugins instead'
complete -f -c composer -n '__fish_composer_using_command update' -l no-autoloader -d 'Skips autoloader generation'
complete -f -c composer -n '__fish_composer_using_command update' -l no-scripts -d 'Skips the execution of all scripts defined in composer.json file'
complete -f -c composer -n '__fish_composer_using_command update' -l no-progress -d 'Do not output download progress'
complete -f -c composer -n '__fish_composer_using_command update' -l no-suggest -d 'Do not show package suggestions'
complete -f -c composer -n '__fish_composer_using_command update' -l with-dependencies -d 'Add deps of whitelisted packages to whitelist, except those defined in root package'
complete -f -c composer -n '__fish_composer_using_command update' -l with-all-dependencies -d 'Add deps of whitelisted packages to whitelist, including those defined in root package'
complete -f -c composer -n '__fish_composer_using_command update' -s v -l verbose -d 'Shows more details including new commits pulled in when updating packages'
complete -f -c composer -n '__fish_composer_using_command update' -s o -l optimize-autoloader -d 'Optimize autoloader during autoloader dump'
complete -f -c composer -n '__fish_composer_using_command update' -s a -l classmap-authoritative -d 'Autoload classes from the classmap only. Implicitly enables `--optimize-autoloader`'
complete -f -c composer -n '__fish_composer_using_command update' -l apcu-autoloader -d 'Use APCu to cache found/not-found classes'
complete -f -c composer -n '__fish_composer_using_command update' -l ignore-platform-reqs -d 'Ignore platform requirements (php & ext- packages)'
complete -f -c composer -n '__fish_composer_using_command update' -l prefer-stable -d 'Prefer stable versions of dependencies'
complete -f -c composer -n '__fish_composer_using_command update' -l prefer-lowest -d 'Prefer lowest versions of dependencies'
complete -f -c composer -n '__fish_composer_using_command update' -s i -l interactive -d 'Interactive interface with autocompletion to select the packages to update'
complete -f -c composer -n '__fish_composer_using_command update' -l root-reqs -d 'Restricts the update to your first degree dependencies'

# validate
complete -f -c composer -n '__fish_composer_using_command validate' -l no-check-all -d 'Do not validate requires for overly strict/loose constraints'
complete -f -c composer -n '__fish_composer_using_command validate' -l no-check-lock -d 'Do not check if lock file is up to date'
complete -f -c composer -n '__fish_composer_using_command validate' -l no-check-publish -d 'Do not check for publish errors'
complete -f -c composer -n '__fish_composer_using_command validate' -l with-dependencies -d 'Also validate the composer.json of all installed dependencies'
complete -f -c composer -n '__fish_composer_using_command validate' -l strict -d 'Return a non-zero exit code for warnings as well as errors'

# why
complete -f -c composer -n '__fish_composer_using_command why' -a "(__fish_composer_installed_packages)"

# why-not
complete -f -c composer -n '__fish_composer_using_command why-not' -a "(__fish_composer_installed_packages)"

# global options
complete -c composer -n __fish_composer_needs_command -s h -l help -d 'Displays composer\'s help'
complete -c composer -n __fish_composer_needs_command -s q -l quiet -d 'Do not output any message'
complete -c composer -n __fish_composer_needs_command -s v -l verbose -d 'Verbose mode (pass multiple times for even verboser mode)'
complete -c composer -n __fish_composer_needs_command -s V -l version -d 'Display composer\'s application version'
complete -c composer -n __fish_composer_needs_command -l ansi -d 'Force ANSI output'
complete -c composer -n __fish_composer_needs_command -l no-ansi -d 'Disable ANSI output'
complete -c composer -n __fish_composer_needs_command -s n -l no-interaction -d 'Do not ask any interactive question'
complete -c composer -n __fish_composer_needs_command -l profile -d 'Display timing and memory usage information'
complete -c composer -n __fish_composer_needs_command -l no-plugins -d 'Whether to disable plugins'
complete -c composer -n __fish_composer_needs_command -s d -l working-dir -d 'If specified, use the given directory as working directory'
complete -c composer -n __fish_composer_needs_command -l no-cache -d 'Prevent use of the cache'
