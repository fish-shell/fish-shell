function __fish_brew_needs_command
  set cmd (commandline -opc)
  if [ (count $cmd) -eq 1 -a $cmd[1] = 'brew' ]
    return 0
  end
  return 1
end

function __fish_brew_using_command
  set cmd (commandline -opc)
  if [ (count $cmd) -gt 1 ]
    if [ $argv[1] = $cmd[2] ]
      return 0
    end
  end
  return 1
end

function __fish_brew_formulae
    set -l formuladir (brew --repository)/Library/Formula/
    # __fish_complete_suffix .rb
    ls $formuladir/*.rb | sed 's/.rb$//' | sed "s|^$formuladir||"
end

############
# commands #
############

# audit
complete -f -c brew -n '__fish_brew_needs_command' -a audit -d 'Check formula'
complete -f -c brew -n '__fish_brew_using_command audit' -a '(__fish_brew_formulae)' -d 'formula'

# cat
complete -f -c brew -n '__fish_brew_needs_command' -a cat -d 'Display formula'
complete -f -c brew -n '__fish_brew_using_command cat' -a '(__fish_brew_formulae)' -d 'formula'

# cleanup
complete -f -c brew -n '__fish_brew_needs_command' -a cleanup -d 'Remove old installed versions'
complete -f -c brew -n '__fish_brew_using_command cleanup' -l force -d 'Remove out-of-date keg-only brews as well'
complete -f -c brew -n '__fish_brew_using_command cleanup' -a '-n' -d 'Dry run'
complete -f -c brew -n '__fish_brew_using_command cleanup' -a '-s' -d 'Scrubs the cache'
complete -f -c brew -n '__fish_brew_using_command cleanup' -a '(__fish_brew_formulae)' -d 'formula'

# create
complete -f -c brew -n '__fish_brew_needs_command' -a create -d 'Create new formula from URL'
complete -f -c brew -n '__fish_brew_using_command create' -a '--cmake' -d 'Use template for CMake-style build'
complete -f -c brew -n '__fish_brew_using_command create' -a '--autotools' -d 'Use template for Autotools-style build'
complete -f -c brew -n '__fish_brew_using_command create' -a '--no-fetch' -d 'Don\'t download URL'

# deps
complete -f -c brew -n '__fish_brew_needs_command' -a deps -d 'Show a formula\'s dependencies'
complete -f -c brew -n '__fish_brew_using_command deps' -a '--1' -d 'Show only 1 level down'
complete -f -c brew -n '__fish_brew_using_command deps' -a '-n' -d 'Show in topological order'
complete -f -c brew -n '__fish_brew_using_command deps' -a '--tree' -d 'Show dependencies as tree'
complete -f -c brew -n '__fish_brew_using_command deps' -a '--all' -d 'Show dependencies for all formulae'
complete -f -c brew -n '__fish_brew_using_command deps' -a '(__fish_brew_formulae)' -d 'formula'

# diy
complete -f -c brew -n '__fish_brew_needs_command' -a 'diy configure' -d 'Determine installation prefix for non-brew software'
complete -f -c brew -n '__fish_brew_using_command diy' -a '--set-name' -d 'Set name of package'
complete -f -c brew -n '__fish_brew_using_command diy' -a '--set-version' -d 'Set version of package'

complete -f -c brew -n '__fish_brew_needs_command' -a 'doctor' -d 'Check your system for problems'
complete -f -c brew -n '__fish_brew_needs_command' -a 'edit' -d 'Open brew/formula for editing'

# fetch
complete -f -c brew -n '__fish_brew_needs_command' -a fetch -d 'Download source for formula'
complete -f -c brew -n '__fish_brew_using_command fetch' -a '--force' -d 'Remove a previously cached version and re-fetch'
complete -f -c brew -n '__fish_brew_using_command fetch' -a '--HEAD' -d 'Download the HEAD version from a VCS'
complete -f -c brew -n '__fish_brew_using_command fetch' -a '--deps' -d 'Also download dependencies'
complete -f -c brew -n '__fish_brew_using_command fetch' -a '-v' -d 'Make HEAD checkout verbose'
complete -f -c brew -n '__fish_brew_using_command fetch' -a '(__fish_brew_formulae)' -d 'formula'

complete -f -c brew -n '__fish_brew_needs_command' -a 'help' -d 'Display help'

# home
complete -f -c brew -n '__fish_brew_needs_command' -a home -d 'Open brew/formula\'s homepage'
complete -c brew -n '__fish_brew_using_command home' -a '(__fish_brew_formulae)' -d 'formula'

# info
complete -f -c brew -n '__fish_brew_needs_command' -a 'info abv' -d 'Display information about formula'
complete -f -c brew -n '__fish_brew_using_command info' -a '--all' -d 'Display info for all formulae'
complete -f -c brew -n '__fish_brew_using_command info' -a '--github' -d 'Open the GitHub History page for formula'
complete -c brew -n '__fish_brew_using_command info' -a '(__fish_brew_formulae)' -d 'formula'

# install
complete -f -c brew -n '__fish_brew_needs_command' -a 'install' -d 'Install formula'
complete -f -c brew -n '__fish_brew_using_command install' -a '--force' -d 'Force install'
complete -f -c brew -n '__fish_brew_using_command install' -a '--debug' -d 'If install fails, open shell in temp directory'
complete -f -c brew -n '__fish_brew_using_command install' -a '--ignore-dependencies' -d 'skip installing any dependencies of any kind'
complete -f -c brew -n '__fish_brew_using_command install' -a '--fresh' -d 'Don\'t re-use any options from previous installs'
complete -f -c brew -n '__fish_brew_using_command install' -a '--use-clang' -d 'Attempt to compile using clang'
complete -f -c brew -n '__fish_brew_using_command install' -a '--use-gcc' -d 'Attempt to compile using GCC'
complete -f -c brew -n '__fish_brew_using_command install' -a '--use-llvm' -d 'Attempt to compile using the LLVM'
complete -f -c brew -n '__fish_brew_using_command install' -a '--build-from-source' -d 'Compile from source even if a bottle is provided'
complete -f -c brew -n '__fish_brew_using_command install' -a '--devel' -d 'Install the development version of formula'
complete -f -c brew -n '__fish_brew_using_command install' -a '--HEAD' -d 'Install the HEAD version from VCS'
complete -f -c brew -n '__fish_brew_using_command install' -a '--interactive' -d 'Download and patch formula, then open a shell'
complete -c brew -n '__fish_brew_using_command install' -a '(__fish_brew_formulae)' -d 'formula'

# link
complete -f -c brew -n '__fish_brew_needs_command' -a 'link ln' -d 'Symlink installed formula'
complete -f -c brew -n '__fish_brew_using_command link' -a '(__fish_brew_formulae)' -d 'formula'
complete -f -c brew -n '__fish_brew_using_command ln' -a '(__fish_brew_formulae)' -d 'formula'

# list
complete -f -c brew -n '__fish_brew_needs_command' -a 'list ls' -d 'List all installed formula'
complete -f -c brew -n '__fish_brew_using_command list' -a '--unbrewed' -d 'List all files in the Homebrew prefix not installed by brew'
complete -f -c brew -n '__fish_brew_using_command list' -a '--versions' -d 'Show the version number'
complete -c brew -n '__fish_brew_using_command list' -a '(__fish_brew_formulae)' -d 'formula'
#ls
complete -f -c brew -n '__fish_brew_using_command ls' -a '--unbrewed' -d 'List all files in the Homebrew prefix not installed by brew'
complete -f -c brew -n '__fish_brew_using_command ls' -a '--versions' -d 'Show the version number'
complete -c brew -n '__fish_brew_using_command ls' -a '(__fish_brew_formulae)' -d 'formula'

# log
complete -f -c brew -n '__fish_brew_needs_command' -a log -d 'Show log for formula'
complete -c brew -n '__fish_brew_using_command log' -a '(__fish_brew_formulae)' -d 'formula'

# missing
complete -f -c brew -n '__fish_brew_needs_command' -a missing -d 'Check formula for missing dependencies'
complete -c brew -n '__fish_brew_using_command missing' -a '(__fish_brew_formulae)' -d 'formula'

# options
complete -f -c brew -n '__fish_brew_needs_command' -a options -d 'Display install options for formula'
complete -c brew -n '__fish_brew_using_command options' -a '(__fish_brew_formulae)' -d 'formula'

# outdated
complete -f -c brew -n '__fish_brew_needs_command' -a outdated -d 'Show formula that have updated versions'
complete -f -c brew -n '__fish_brew_using_command outdated' -a '--quiet' -d 'Display only names'

# prune
complete -f -c brew -n '__fish_brew_needs_command' -a prune -d 'Remove dead symlinks'

# search
complete -f -c brew -n '__fish_brew_needs_command' -a 'search -S' -d 'Search for formula by name'
complete -f -c brew -n '__fish_brew_using_command search' -a '--macports' -d 'Search on MacPorts'
complete -f -c brew -n '__fish_brew_using_command search' -a '--fink' -d 'Search on Fink'
complete -f -c brew -n '__fish_brew_using_command -S' -a '--macports' -d 'Search on MacPorts'
complete -f -c brew -n '__fish_brew_using_command -S' -a '--fink' -d 'Search on Fink'

# tap
complete -f -c brew -n '__fish_brew_needs_command' -a tap -d 'Tap a new formula repository on GitHub'

# test
complete -f -c brew -n '__fish_brew_needs_command' -a test -d 'Run tests for formula'
complete -c brew -n '__fish_brew_using_command test' -a '(__fish_brew_formulae)' -d 'formula'

# uninstall
complete -f -c brew -n '__fish_brew_needs_command' -a 'uninstall remove rm' -d 'Uninstall formula'
complete -c brew -n '__fish_brew_using_command uninstall' -a '(__fish_brew_formulae)' -d 'formula'
complete -c brew -n '__fish_brew_using_command remove' -a '(__fish_brew_formulae)' -d 'formula'
complete -c brew -n '__fish_brew_using_command rm' -a '(__fish_brew_formulae)' -d 'formula'
complete -f -c brew -n '__fish_brew_using_command uninstall' -a '--force' -d 'Delete all installed versions'
complete -f -c brew -n '__fish_brew_using_command remove' -a '--force' -d 'Delete all installed versions'
complete -f -c brew -n '__fish_brew_using_command rm' -a '--force' -d 'Delete all installed versions'

# unlink
complete -f -c brew -n '__fish_brew_needs_command' -a unlink -d 'Unlink formula'
complete -c brew -n '__fish_brew_using_command unlink' -a '(__fish_brew_formulae)' -d 'formula'

# untap
complete -f -c brew -n '__fish_brew_needs_command' -a untap -d 'Remove a tapped repository'

# update
complete -f -c brew -n '__fish_brew_needs_command' -a update -d 'Fetch newest version of Homebrew and formulas'
complete -f -c brew -n '__fish_brew_using_command update' -a '--rebase' -d 'Use git pull --rebase'

# upgrade
complete -f -c brew -n '__fish_brew_needs_command' -a upgrade -d 'Upgrade outdated brews'
complete -c brew -n '__fish_brew_using_command upgrade' -a '(__fish_brew_formulae)' -d 'formula'

# uses
complete -f -c brew -n '__fish_brew_needs_command' -a uses -d 'Show formulas that depend on specified formula'
complete -f -c brew -n '__fish_brew_using_command uses' -a '--installed' -d 'List only installed formulae'
complete -c brew -n '__fish_brew_using_command uses' -a '(__fish_brew_formulae)' -d 'formula'

# versions
complete -f -c brew -n '__fish_brew_needs_command' -a versions -d 'List previous versions of formula'
complete -f -c brew -n '__fish_brew_using_command versions' -a '--compact' -d 'Show all options on a single line'
complete -c brew -n '__fish_brew_using_command versions' -a '(__fish_brew_formulae)' -d 'formula'


############
# switches #
############
complete -f -c brew -n '__fish_brew_needs_command' -a '-v --version' -d 'Print version number of brew'
complete -f -c brew -n '__fish_brew_needs_command' -l repository -x -d 'Display where Homebrew\'s .git directory is located'
complete -f -c brew -n '__fish_brew_needs_command' -l config -x -d 'Show Homebrew and system configuration'

# --prefix
complete -f -c brew -n '__fish_brew_needs_command' -l prefix -d 'Display Homebrew\'s install path'
complete -f -c brew -n '__fish_brew_using_command --prefix' -a '(__fish_brew_formulae)' -d 'Display formula\'s install path'

# --cache
complete -f -c brew -n '__fish_brew_needs_command' -l cache -d 'Display Homebrew\'s download cache'
complete -f -c brew -n '__fish_brew_needs_command' -n '__fish_brew_using_command --cache' -a '(__fish_brew_formulae)' -d 'Display the file or directory used to cache formula'

# --cellar
complete -f -c brew -n '__fish_brew_needs_command' -l cellar -d 'Display Homebrew\'s Cellar path'
complete -f -c brew -n '__fish_brew_using_command --cellar' -a '(__fish_brew_formulae)' -d 'Display formula\'s install path in Cellar'

