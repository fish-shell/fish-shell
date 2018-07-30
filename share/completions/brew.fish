function __fish_brew_get_cmd
  for c in (commandline -opc)
    if not string match -q -- '-*' $c
      echo $c
    end
  end
end

function __fish_brew_needs_command
  set cmd (__fish_brew_get_cmd)
  if not set -q cmd[2]
    return 0
  end
  return 1
end

function __fish_brew_using_command
  set index 2

  if set -q argv[2]
    set index $argv[2]
  end

  set cmd (__fish_brew_get_cmd)

  if set -q cmd[$index]
    if [ $argv[1] = $cmd[$index] ]
      return 0
    end
  end
  return 1
end

function __fish_brew_formulae
    # list all local formula, do not use `brew search some_text` against searching online
    # TODO fix the problem with `tap-pin`, tap-pin will modify the priority
    # If you pin your custom tap for VIM, you should
    # `brew install homebrew/core/vim` to install VIM from `core` repo
    # `brew install vim` to install VIM from more prior repo
    # but `brew search` won't change display for custom VIM and core VIM
    # 'vim' for core VIM
    # 'custUser/custRepo/vim' for more prior VIM
    # more info: https://github.com/Homebrew/brew/blob/master/share/doc/homebrew/brew-tap.md#formula-duplicate-names
    brew search
end

function __fish_brew_installed_formulas
    brew list
end

function __fish_brew_leaves
    brew leaves
end

function __fish_brew_outdated_formulas
    brew outdated
end

function __fish_brew_pinned_formulas
    brew list --pinned
end

function __fish_brew_taps
    brew tap
end


###########################
# subcommand initializing #
###########################

########
# cask #
########

function __fish_brew_is_subcommand_cask
  if __fish_brew_using_command cask
    for action in $argv
      if __fish_brew_using_command $action 3
        return 0
      end
    end
  end
  return 1
end

function __fish_brew_needs_cask_action
  if __fish_brew_using_command cask
    set cmd (__fish_brew_get_cmd)
    if not set -q cmd[3]
      return 0
    end
  end
  return 1
end

function __fish_brew_casks
  brew search --casks
end

function __fish_brew_casks_installed
  brew cask list
end


############
# services #
############

function __fish_brew_is_subcommand_services
  if __fish_brew_using_command services
    for action in $argv
      if __fish_brew_using_command $action 3
        return 0
      end
    end
  end
  return 1
end

function __fish_brew_needs_services_action
  if __fish_brew_using_command services
    set cmd (__fish_brew_get_cmd)
    if not set -q cmd[3]
      return 0
    end
  end
  return 1
end

function __fish_brew_services
  brew services list | awk '{if (NR>1) print $1}'
end


#####################
# brew commands     #
# (brew cask below) #
#####################

# audit
complete -f -c brew -n '__fish_brew_needs_command' -a audit -d 'Check formula'
complete -f -c brew -n '__fish_brew_using_command audit' -a '(__fish_brew_formulae)'

# bottle
complete -f -c brew -n '__fish_brew_needs_command' -a bottle -d 'Create a binary package'
complete -f -c brew -n '__fish_brew_using_command bottle' -l 'homebrew-developer' -d 'Output developer debug information'
complete -f -c brew -n '__fish_brew_using_command bottle' -l 'no-revision' -d 'Do not bump the bottle revision number'
complete -f -c brew -n '__fish_brew_using_command bottle' -l 'rb' -d 'Write bottle block to a Ruby source file'
complete -f -c brew -n '__fish_brew_using_command bottle' -l 'write' -d 'Write bottle block to formula file'
complete -f -c brew -n '__fish_brew_using_command bottle' -l 'merge' -d 'Merge multiple bottle outputs'

# cask
complete -f -c brew -n '__fish_brew_needs_command' -a cask -d 'Manage installed casks'

# cat
complete -f -c brew -n '__fish_brew_needs_command' -a cat -d 'Display formula'
complete -f -c brew -n '__fish_brew_using_command cat' -a '(__fish_brew_formulae)'

# cleanup
complete -f -c brew -n '__fish_brew_needs_command' -a cleanup -d 'Remove old installed versions'
complete -f -c brew -n '__fish_brew_using_command cleanup' -l force -d 'Remove out-of-date keg-only brews as well'
complete -f -c brew -n '__fish_brew_using_command cleanup' -l dry-run -d 'Show what files would be removed'
complete -f -c brew -n '__fish_brew_using_command cleanup' -s n -d 'Show what files would be removed'
complete -f -c brew -n '__fish_brew_using_command cleanup' -s s -d 'Scrub the cache'
complete -f -c brew -n '__fish_brew_using_command cleanup' -a '(__fish_brew_installed_formulas)'

# create
complete -f -c brew -n '__fish_brew_needs_command' -a create -d 'Create new formula from URL'
complete -f -c brew -n '__fish_brew_using_command create' -l cmake -d 'Use template for CMake-style build'
complete -f -c brew -n '__fish_brew_using_command create' -l autotools -d 'Use template for Autotools-style build'
complete -f -c brew -n '__fish_brew_using_command create' -l no-fetch -d 'Don\'t download URL'
complete -f -c brew -n '__fish_brew_using_command create' -l set-name -d 'Override name autodetection'
complete -f -c brew -n '__fish_brew_using_command create' -l set-version -d 'Override version autodetection'

# desc
complete -f -c brew -n '__fish_brew_needs_command' -a desc -d "Summarize specified formulae in one line"
complete -f -c brew -n '__fish_brew_using_command desc' -l search -d 'Search names and descriptions'
complete -f -c brew -n '__fish_brew_using_command desc' -l name -d 'Search only names'
complete -f -c brew -n '__fish_brew_using_command desc' -l description -d 'Search only descriptions'
complete -f -c brew -n '__fish_brew_using_command desc' -a '(__fish_brew_formulae)'

# deps
complete -f -c brew -n '__fish_brew_needs_command' -a deps -d 'Show a formula\'s dependencies'
complete -f -c brew -n '__fish_brew_using_command deps' -l 1 -d 'Show only 1 level down'
complete -f -c brew -n '__fish_brew_using_command deps' -s n -d 'Show in topological order'
complete -f -c brew -n '__fish_brew_using_command deps' -l tree -d 'Show dependencies as tree'
complete -f -c brew -n '__fish_brew_using_command deps' -l all -d 'Show dependencies for all formulae'
complete -f -c brew -n '__fish_brew_using_command deps' -l installed -d 'Show dependencies for installed formulae'
complete -f -c brew -n '__fish_brew_using_command deps' -a '(__fish_brew_formulae)'

# diy
complete -f -c brew -n '__fish_brew_needs_command' -a 'diy configure' -d 'Determine installation prefix for non-brew software'
complete -f -c brew -n '__fish_brew_using_command diy' -l set-name -d 'Set name of package'
complete -f -c brew -n '__fish_brew_using_command diy' -l set-version -d 'Set version of package'

complete -f -c brew -n '__fish_brew_needs_command' -a 'doctor' -d 'Check your system for problems'

# edit
complete -f -c brew -n '__fish_brew_needs_command' -a 'edit' -d 'Open brew/formula for editing'
complete -f -c brew -n '__fish_brew_using_command edit' -a '(__fish_brew_formulae)'

# fetch
complete -f -c brew -n '__fish_brew_needs_command' -a fetch -d 'Download source for formula'
complete -f -c brew -n '__fish_brew_using_command fetch' -l force -d 'Remove a previously cached version and re-fetch'
complete -f -c brew -n '__fish_brew_using_command fetch' -l HEAD -d 'Download the HEAD version from a VCS'
complete -f -c brew -n '__fish_brew_using_command fetch' -l deps -d 'Also download dependencies'
complete -f -c brew -n '__fish_brew_using_command fetch' -s v -d 'Make HEAD checkout verbose'
complete -f -c brew -n '__fish_brew_using_command fetch' -l build-from-source -d 'Fetch source package instead of bottle'
complete -f -c brew -n '__fish_brew_using_command fetch' -a '(__fish_brew_formulae)'

complete -f -c brew -n '__fish_brew_needs_command' -a 'help' -d 'Display help'

# home
complete -f -c brew -n '__fish_brew_needs_command' -a home -d 'Open brew/formula\'s homepage'
complete -c brew -n '__fish_brew_using_command home' -a '(__fish_brew_formulae)'

# info
complete -f -c brew -n '__fish_brew_needs_command' -a 'info abv' -d 'Display information about formula'
complete -f -c brew -n '__fish_brew_using_command info' -l all -d 'Display info for all formulae'
complete -f -c brew -n '__fish_brew_using_command info' -l github -d 'Open the GitHub History page for formula'
complete -c brew -n '__fish_brew_using_command info' -a '(__fish_brew_formulae)'

# install
complete -f -c brew -n '__fish_brew_needs_command' -a 'install' -d 'Install formula'
complete -f -c brew -n '__fish_brew_using_command install' -l force -d 'Force install'
complete -f -c brew -n '__fish_brew_using_command install' -l debug -d 'If install fails, open shell in temp directory'
complete -f -c brew -n '__fish_brew_using_command install' -l ignore-dependencies -d 'skip installing any dependencies of any kind'
complete -f -c brew -n '__fish_brew_using_command install' -l cc -a "clang gcc-4.0 gcc-4.2 gcc-4.3 gcc-4.4 gcc-4.5 gcc-4.6 gcc-4.7 gcc-4.8 gcc-4.9 llvm-gcc" -d 'Attempt to compile using the specified compiler'
complete -f -c brew -n '__fish_brew_using_command install' -l build-from-source -d 'Compile from source even if a bottle is provided'
complete -f -c brew -n '__fish_brew_using_command install' -l devel -d 'Install the development version of formula'
complete -f -c brew -n '__fish_brew_using_command install' -l HEAD -d 'Install the HEAD version from VCS'
complete -f -c brew -n '__fish_brew_using_command install' -l interactive -d 'Download and patch formula, then open a shell'
complete -f -c brew -n '__fish_brew_using_command install' -l env -a "std super" -d 'Force the specified build environment'
complete -f -c brew -n '__fish_brew_using_command install' -l build-bottle -d 'Optimize for a generic CPU architecture'
complete -f -c brew -n '__fish_brew_using_command install' -l bottle-arch -a 'core core2 penryn g3 g4 g4e g5' -d 'Optimize for the specified CPU architecture'
complete -c brew -n '__fish_brew_using_command install' -a '(__fish_brew_formulae)'

# leaves
complete -f -c brew -n '__fish_brew_needs_command' -a 'leaves' -d 'List installed top level formulae'
complete -f -c brew -n '__fish_brew_using_command leaves' -a '(__fish_brew_leaves)'

# link
complete -f -c brew -n '__fish_brew_needs_command' -a 'link ln' -d 'Symlink installed formula'
complete -f -c brew -n '__fish_brew_using_command link' -l overwrite -d 'Overwrite existing files'
complete -f -c brew -n '__fish_brew_using_command ln' -l overwrite -d 'Overwrite existing files'
complete -f -c brew -n '__fish_brew_using_command link' -l dry-run -d 'Show what files would be linked or overwritten'
complete -f -c brew -n '__fish_brew_using_command ln' -l dry-run -d 'Show what files would be linked or overwritten'
complete -f -c brew -n '__fish_brew_using_command link' -l force -d 'Allow keg-only formulae to be linked'
complete -f -c brew -n '__fish_brew_using_command ln' -l force -d 'Allow keg-only formulae to be linked'
complete -f -c brew -n '__fish_brew_using_command link' -a '(__fish_brew_installed_formulas)'
complete -f -c brew -n '__fish_brew_using_command ln' -a '(__fish_brew_installed_formulas)'

# linkapps
complete -f -c brew -n '__fish_brew_needs_command' -a linkapps -d 'Symlink .app bundles into /Applications'
complete -f -c brew -n '__fish_brew_using_command linkapps' -l local -d 'Link .app bundles into ~/Applications instead'

# list
complete -f -c brew -n '__fish_brew_needs_command' -a 'list ls' -d 'List all installed formula'
complete -f -c brew -n '__fish_brew_using_command list' -l unbrewed -d 'List all files in the Homebrew prefix not installed by brew'
complete -f -c brew -n '__fish_brew_using_command list' -l versions -d 'Show the version number'
complete -f -c brew -n '__fish_brew_using_command list' -l pinned -d 'Show the versions of pinned formulae'
complete -c brew -n '__fish_brew_using_command list' -a '(__fish_brew_formulae)'

# ls
complete -f -c brew -n '__fish_brew_using_command ls' -l unbrewed -d 'List all files in the Homebrew prefix not installed by brew'
complete -f -c brew -n '__fish_brew_using_command ls' -l versions -d 'Show the version number'
complete -f -c brew -n '__fish_brew_using_command ls' -l pinned -d 'Show the versions of pinned formulae'
complete -c brew -n '__fish_brew_using_command ls' -a '(__fish_brew_formulae)'

# log
complete -f -c brew -n '__fish_brew_needs_command' -a log -d 'Show log for formula'
complete -c brew -n '__fish_brew_using_command log' -a '(__fish_brew_formulae)' -d 'formula'

# missing
complete -f -c brew -n '__fish_brew_needs_command' -a missing -d 'Check formula for missing dependencies'
complete -c brew -n '__fish_brew_using_command missing' -a '(__fish_brew_formulae)' -d 'formula'

# options
complete -f -c brew -n '__fish_brew_needs_command' -a options -d 'Display install options for formula'
complete -f -c brew -n '__fish_brew_using_command options' -l compact -d 'Show all options as a space-delimited list'
complete -f -c brew -n '__fish_brew_using_command options' -l all -d 'Show options for all formulae'
complete -f -c brew -n '__fish_brew_using_command options' -l installed -d 'Show options for all installed formulae'
complete -c brew -n '__fish_brew_using_command options' -a '(__fish_brew_formulae)' -d 'formula'

# outdated
complete -f -c brew -n '__fish_brew_needs_command' -a outdated -d 'Show formula that have updated versions'
complete -f -c brew -n '__fish_brew_using_command outdated' -l quiet -d 'Display only names'

# pin
complete -f -c brew -n '__fish_brew_needs_command' -a pin -d 'Pin the specified formulae to their current versions'
complete -f -c brew -n '__fish_brew_using_command pin' -a '(__fish_brew_installed_formulas)' -d 'formula'

# prune
complete -f -c brew -n '__fish_brew_needs_command' -a prune -d 'Remove dead symlinks'

# search
complete -f -c brew -n '__fish_brew_needs_command' -a 'search -S' -d 'Search for formula by name'
complete -f -c brew -n '__fish_brew_using_command search' -l macports -d 'Search on MacPorts'
complete -f -c brew -n '__fish_brew_using_command search' -l fink -d 'Search on Fink'
complete -f -c brew -n '__fish_brew_using_command -S' -l macports -d 'Search on MacPorts'
complete -f -c brew -n '__fish_brew_using_command -S' -l fink -d 'Search on Fink'

# services
complete -f -c brew -n '__fish_brew_needs_command' -a services -d 'Manage Homebrew services'
complete -f -c brew -n '__fish_brew_needs_services_action' -a cleanup -d 'Get rid of stale services and unused plist'
complete -f -c brew -n '__fish_brew_needs_services_action' -a list -d 'List all services managed by Homebrew'
complete -f -c brew -n '__fish_brew_needs_services_action' -a restart -d 'Gracefully restart a service'
complete -f -c brew -n '__fish_brew_needs_services_action' -a start -d 'Start a service'
complete -f -c brew -n '__fish_brew_needs_services_action' -a stop -d 'Stop a service'
complete -f -c brew -n '__fish_brew_is_subcommand_services restart start stop' -a '(__fish_brew_services)' -d 'formula'
complete -f -c brew -n '__fish_brew_is_subcommand_services restart start stop' -l all -d 'All Services'

# sh
complete -f -c brew -n '__fish_brew_needs_command' -a sh -d 'Instantiate a Homebrew build enviornment'
complete -f -c brew -n '__fish_brew_using_command sh' -l env=std -d 'Use stdenv instead of superenv'

# tap
complete -f -c brew -n '__fish_brew_needs_command' -a tap -d 'Tap a new formula repository on GitHub'
complete -f -c brew -n '__fish_brew_using_command tap' -l repair -d 'Create and prune tap symlinks as appropriate'

# test
complete -f -c brew -n '__fish_brew_needs_command' -a test -d 'Run tests for formula'
complete -f -c brew -n '__fish_brew_using_command test' -a '(__fish_brew_installed_formulas)' -d 'formula'

# uninstall
complete -f -c brew -n '__fish_brew_needs_command' -a 'uninstall remove rm' -d 'Uninstall formula'
complete -f -c brew -n '__fish_brew_using_command uninstall' -a '(__fish_brew_installed_formulas)'
complete -f -c brew -n '__fish_brew_using_command remove' -a '(__fish_brew_installed_formulas)'
complete -f -c brew -n '__fish_brew_using_command rm' -a '(__fish_brew_installed_formulas)'
complete -f -c brew -n '__fish_brew_using_command uninstall' -l force -d 'Delete all installed versions'
complete -f -c brew -n '__fish_brew_using_command remove' -l force -d 'Delete all installed versions'
complete -f -c brew -n '__fish_brew_using_command rm' -l force -d 'Delete all installed versions'

# unlink
complete -f -c brew -n '__fish_brew_needs_command' -a unlink -d 'Unlink formula'
complete -f -c brew -n '__fish_brew_using_command unlink' -a '(__fish_brew_installed_formulas)'

# unlinkapps
complete -f -c brew -n '__fish_brew_needs_command' -a unlinkapps -d 'Remove links created by brew linkapps'
complete -f -c brew -n '__fish_brew_using_command unlinkapps' -l local -d 'Remove links from ~/Applications created by brew linkapps'

# unpack
complete -f -c brew -n '__fish_brew_needs_command' -a unpack -d 'Extract source code'
complete -c brew -n '__fish_brew_using_command unpack' -a '(__fish_brew_formulae)'

# unpin
complete -f -c brew -n '__fish_brew_needs_command' -a unpin -d 'Unpin specified formulae'
complete -f -c brew -n '__fish_brew_using_command unpin' -a '(__fish_brew_pinned_formulas)'

# untap
complete -f -c brew -n '__fish_brew_needs_command' -a untap -d 'Remove a tapped repository'
complete -f -c brew -n '__fish_brew_using_command untap' -a '(__fish_brew_taps)'

# update
complete -f -c brew -n '__fish_brew_needs_command' -a update -d 'Fetch newest version of Homebrew and formulas'
complete -f -c brew -n '__fish_brew_using_command update' -l rebase -d 'Use git pull --rebase'

# upgrade
complete -f -c brew -n '__fish_brew_needs_command' -a upgrade -d 'Upgrade outdated brews'
complete -f -c brew -n '__fish_brew_using_command upgrade' -a '(__fish_brew_outdated_formulas)'

# uses
complete -f -c brew -n '__fish_brew_needs_command' -a uses -d 'Show formulas that depend on specified formula'
complete -f -c brew -n '__fish_brew_using_command uses' -l installed -d 'List only installed formulae'
complete -f -c brew -n '__fish_brew_using_command uses' -l recursive -d 'Resolve more than one level of dependencies'
complete -c brew -n '__fish_brew_using_command uses' -a '(__fish_brew_formulae)'


#################
# brew switches #
#################

complete -f -c brew -n '__fish_brew_needs_command' -a '-v --version' -d 'Print version number of brew'
complete -f -c brew -n '__fish_brew_needs_command' -l env -x -d 'Show Homebrew a summary of the build environment'
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


######################
# brew cask commands #
#####################

# audit
complete -f -c brew -n '__fish_brew_needs_cask_action' -a audit -d 'Audit casks, add token to audit specific cask'
complete -f -c brew -n '__fish_brew_is_subcommand_cask audit' -a '(__fish_brew_casks)'

# cat
complete -f -c brew -n '__fish_brew_needs_cask_action' -a cat -d 'Display cask'
complete -f -c brew -n '__fish_brew_is_subcommand_cask cat' -a '(__fish_brew_casks)'


# cleanup
complete -f -c brew -n '__fish_brew_needs_cask_action' -a cleanup -d 'Cleans up cached downloads and tracker symlinks'
complete -f -c brew -n '__fish_brew_is_subcommand_cask cleanup' -l outdated -d 'Only clean cached downloads older than 10 days'

# create
complete -f -c brew -n '__fish_brew_needs_cask_action' -a create -d 'Create a new cask'

# doctor/dr
complete -f -c brew -n '__fish_brew_needs_cask_action' -a 'doctor dr' -d 'Checks for configuration issues'

# edit
complete -f -c brew -n '__fish_brew_needs_cask_action' -a edit -d 'Edit cask'
complete -f -c brew -n '__fish_brew_is_subcommand_cask edit' -a '(__fish_brew_casks)'

# fetch
complete -f -c brew -n '__fish_brew_needs_cask_action' -a fetch -d 'Fetch cask'
complete -f -c brew -n '__fish_brew_is_subcommand_cask fetch' -l force -d 'Force a redownload of cask'
complete -f -c brew -n '__fish_brew_is_subcommand_cask fetch' -a '(__fish_brew_casks)'

# home
complete -f -c brew -n '__fish_brew_needs_cask_action' -a 'home homepage' -d 'Open cask/cask\'s homepage'
complete -f -c brew -n '__fish_brew_is_subcommand_cask home' -a '(__fish_brew_casks)'

# homepage
complete -f -c brew -n '__fish_brew_is_subcommand_cask homepage' -a '(__fish_brew_casks_installed)'

# info
complete -f -c brew -n '__fish_brew_needs_cask_action' -a 'info abv' -d 'Dislay info about cask'
complete -f -c brew -n '__fish_brew_is_subcommand_cask info' -a '(__fish_brew_casks)'
# adv
complete -f -c brew -n '__fish_brew_is_subcommand_cask abv' -a '(__fish_brew_casks)'

# install
complete -f -c brew -n '__fish_brew_needs_cask_action' -a install -d 'Install cask indentified by token'
complete -f -c brew -n '__fish_brew_is_subcommand_cask install' -l force -d 'Force install of cask'
complete -f -c brew -n '__fish_brew_is_subcommand_cask install' -l require-sha -d 'Require SHA, fails otherwise'
complete -f -c brew -n '__fish_brew_is_subcommand_cask install' -l skip-cask-deps -d 'Skip any cask dependencies'
complete -c brew -n '__fish_brew_is_subcommand_cask install' -a '(__fish_brew_casks)'

# list
complete -f -c brew -n '__fish_brew_needs_cask_action' -a 'list ls' -d 'List installed casks'
complete -f -c brew -n '__fish_brew_is_subcommand_cask list' -s 1 -d 'Format output as a single column'
complete -f -c brew -n '__fish_brew_is_subcommand_cask list' -l versions -d 'Show all installed versions'
complete -f -c brew -n '__fish_brew_is_subcommand_cask list' -a '(__fish_brew_casks_installed)' -d 'List staged files'

# ls
complete -f -c brew -n '__fish_brew_is_subcommand_cask ls' -s 1 -d 'Format output as a single column'
complete -f -c brew -n '__fish_brew_is_subcommand_cask ls' -l versions -d 'Show all installed versions'
complete -f -c brew -n '__fish_brew_is_subcommand_cask ls' -a '(__fish_brew_casks_installed)' -d 'List staged files'

# outdated
complete -f -c brew -n '__fish_brew_needs_cask_action' -a outdated -d 'List outdated casks'
complete -f -c brew -n '__fish_brew_is_subcommand_cask outdated' -l greedy -d 'Include casks having auto_updates true or version :latest'
complete -f -c brew -n '__fish_brew_is_subcommand_cask outdated' -l quiet -d 'Suppresses the display of versions'
complete -f -c brew -n '__fish_brew_is_subcommand_cask outdated' -l verbose -d 'Forces the display of the outdated and latest version'
complete -f -c brew -n '__fish_brew_is_subcommand_cask outdated' -a '(__fish_brew_casks_installed)'

# reinstall
complete -f -c brew -n '__fish_brew_needs_cask_action' -a reinstall -d 'Reinstall cask'
complete -f -c brew -n '__fish_brew_is_subcommand_cask reinstall' -a '(__fish_brew_casks_installed)'

# search
complete -f -c brew -n '__fish_brew_needs_cask_action' -a 'search -S' -d 'Search all known casks. RexEx by delimiting using /regex/'

# style
complete -f -c brew -n '__fish_brew_needs_cask_action' -a style -d 'Check all or the given casks for correct style using RuboCop'
complete -f -c brew -n '__fish_brew_is_subcommand_cask style' -l fix -d 'Auto-correct any style errors if possible'
complete -f -c brew -n '__fish_brew_is_subcommand_cask style' -a '(__fish_brew_casks)'

# uninstall
complete -f -c brew -n '__fish_brew_needs_cask_action' -a 'remove rm uninstall' -d 'Uninstall cask'
complete -f -c brew -n '__fish_brew_is_subcommand_cask uninstall' -l force -d 'Force the uninstall'
complete -f -c brew -n '__fish_brew_is_subcommand_cask uninstall' -a '(__fish_brew_casks_installed)'
# remove
complete -f -c brew -n '__fish_brew_is_subcommand_cask remove' -l force -d 'Force the uninstall'
complete -f -c brew -n '__fish_brew_is_subcommand_cask remove' -a '(__fish_brew_casks_installed)'
# rm
complete -f -c brew -n '__fish_brew_is_subcommand_cask rm' -l force -d 'Force the uninstall'
complete -f -c brew -n '__fish_brew_is_subcommand_cask rm' -a '(__fish_brew_casks_installed)'

# upgrade
complete -f -c brew -n '__fish_brew_needs_cask_action' -a upgrade -d 'Upgrade all or given cask'
complete -f -c brew -n '__fish_brew_is_subcommand_cask upgrade' -l force -d 'Force upgrade'
complete -f -c brew -n '__fish_brew_is_subcommand_cask upgrade' -l greedy -d 'Include casks having auto_updates true or version :latest'
complete -f -c brew -n '__fish_brew_is_subcommand_cask upgrade' -a '(__fish_brew_casks_installed)'

# zap
complete -f -c brew -n '__fish_brew_needs_cask_action' -a zap -d 'Unconditionally remove all files associated with the given Cask'
complete -f -c brew -n '__fish_brew_is_subcommand_cask zap' -a '(__fish_brew_casks_installed)'


######################
# brew cask switches #
######################

# version
complete -f -c brew -n '__fish_brew_needs_cask_action' -a --version -d 'Print version number of Caskroom'
