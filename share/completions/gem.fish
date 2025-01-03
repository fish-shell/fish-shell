# Completions for rubygem's `gem' command

# Author: Tassilo Horn <tassilo@member.fsf.org>

#####
# Global options
complete -c gem -n __fish_use_subcommand -s h -l help -d "Print usage informations and quit"
complete -c gem -n __fish_use_subcommand -s v -l version -d "Print the version and quit"

#####
# Subcommands
complete -c gem -n __fish_use_subcommand -xa build -d "Build a gem from a gemspec"
complete -c gem -n __fish_use_subcommand -xa cert -d "Adjust RubyGems certificate settings"
complete -c gem -n __fish_use_subcommand -xa check -d "Check installed gems"
complete -c gem -n __fish_use_subcommand -xa cleanup -d "Cleanup old versions of installed gems in the local repository"
complete -c gem -n __fish_use_subcommand -xa contents -d "Display the contents of the installed gems"
complete -c gem -n __fish_use_subcommand -xa dependency -d "Show the dependencies of an installed gem"
complete -c gem -n __fish_use_subcommand -xa environment -d "Display RubyGems environmental information"
complete -c gem -n __fish_use_subcommand -xa fetch -d "Download a gem into current directory"
complete -c gem -n __fish_use_subcommand -xa help -d "Provide help on the 'gem' command"
complete -c gem -n __fish_use_subcommand -xa install -d "Install a gem into the local repository"
complete -c gem -n __fish_use_subcommand -xa list -d "Display all gems whose name starts with STRING"
complete -c gem -n __fish_use_subcommand -xa query -d "Query gem information in local or remote repositories"
complete -c gem -n __fish_use_subcommand -xa rdoc -d "Generates RDoc for pre-installed gems"
complete -c gem -n __fish_use_subcommand -xa search -d "Display all gems whose name contains STRING"
complete -c gem -n __fish_use_subcommand -xa specification -d "Display gem specification (in YAML)"
complete -c gem -n __fish_use_subcommand -xa uninstall -d "Uninstall a gem from the local repository"
complete -c gem -n __fish_use_subcommand -xa unpack -d "Unpack an installed gem to the current directory"
complete -c gem -n __fish_use_subcommand -xa update -d "Update the named gem or all installed gems in the local repository"

#####
# Subcommand switches

# common opts
set -l common_opt -c gem -n 'not __fish_use_subcommand'
complete $common_opt -l source -d "Use URL as the remote source for gems" -x
complete $common_opt -s p -l http-proxy -d "Use the given HTTP proxy for remote operations" -x
complete $common_opt -l no-http-proxy -d "Use no HTTP proxy for remote operations"
complete $common_opt -s h -l help -d "Get help on this command"
complete $common_opt -s v -l verbose -d "Set the verbose level of output"
complete $common_opt -l config-file -d "Use this config file instead of default" -x
complete $common_opt -l backtrace -d "Show stack backtrace on errors"
complete $common_opt -l debug -d "Turn on Ruby debugging"

##
# cert
set -l cert_opt -c gem -n 'contains cert (commandline -pxc)'
complete $cert_opt -s a -l add -d "Add a trusted certificate" -x
complete $cert_opt -s l -l list -d "List trusted certificates"
complete $cert_opt -s r -l remove -d "Remove trusted certificates containing STRING" -x
complete $cert_opt -s b -l build -d "Build private key and self-signed certificate for EMAIL_ADDR" -x
complete $cert_opt -s C -l certificate -d "Certificate for --sign command" -x
complete $cert_opt -s K -l private-key -d "Private key for --sign command" -x
complete $cert_opt -s s -l sign -d "Sign a certificate with my key and certificate" -x

##
# check
set -l check_opt -c gem -n 'contains check (commandline -pxc)'
complete $check_opt -s v -l verify -d "Verify gem file against its internal checksum" -x
complete $check_opt -s a -l alien -d "Report 'unmanaged' or rogue files in the gem repository"
complete $check_opt -s t -l test -d "Run unit tests for gem"
complete $check_opt -s V -l version -d "Specify version for which to run unit tests"

##
# cleanup
set -l cleanup_opt -c gem -n 'contains cleanup (commandline -pxc)'
complete $cleanup_opt -s d -l dryrun -d "Don't really cleanup"

##
# contents
set -l contents_opt -c gem -n 'contains contents (commandline -pxc)'
complete $contents_opt -s l -l list -d "List the files inside a Gem"
complete $contents_opt -s V -l version -d "Specify version for gem to view"
complete $contents_opt -s s -l spec-dir -d "Search for gems under specific paths" -x
complete $contents_opt -s v -l verbose -d "Be verbose when showing status"

##
# dependency
set -l dep_opt -c gem -n 'contains dependency (commandline -pxc)'
complete $dep_opt -s v -l version -d "Specify version of gem to uninstall" -x
complete $dep_opt -s r -l reverse-dependencies -d "Include reverse dependencies in the output"
complete $dep_opt -l no-reverse-dependencies -d "Don't include reverse dependencies in the output"
complete $dep_opt -s p -l pipe -d "Pipe Format (name --version ver)"

##
# environment
set -l env_opt -c gem -n 'contains environment (commandline -pxc)'
complete $env_opt -xa "packageversion\t'display the package version' gemdir\t'display the path where gems are installed' gempath\t'display path used to search for gems' version\t'display the gem format version' remotesources\t'display the remote gem servers'"

set -l fetch_opt -c gem -n 'contains fetch (commandline -pxc)'
complete $fetch_opt -s v -l version -d "Specify version of gem to download" -x

##
# help
set -l help_opt -c gem -n 'contains help (commandline -pxc)'
complete $help_opt -xa "commands\t'list all gem commands' examples\t'show some examples of usage' build cert check cleanup contents dependency environment help install list query rdoc search specification uninstall unpack update"

##
# install
set -l install_opt -c gem -n 'contains install (commandline -pxc)'
complete $install_opt -s v -l version -d "Specify version of gem to install" -x
complete $install_opt -s l -l local -d "Restrict operations to the LOCAL domain (default)"
complete $install_opt -s r -l remote -d "Restrict operations to the REMOTE domain"
complete $install_opt -s b -l both -d "Allow LOCAL and REMOTE operations"
complete $install_opt -s i -l install-dir -d "Gem repository directory to get installed gems" -x
complete $install_opt -s i -l user-install -d "Install in user's home directory instead of GEM_HOME."
complete $install_opt -s N -l no-document -d "Disable documentation generation on install"
complete $install_opt -l document -a '(__fish_append , rdoc ri)' -d "Specify the documentation types you wish to generate"
complete $install_opt -s f -l force -d "Force gem to install, bypassing dependency checks"
complete $install_opt -l no-force -d "Don't force gem to install, bypassing dependency checks"
complete $install_opt -s t -l test -d "Run unit tests prior to installation"
complete $install_opt -l no-test -d "Don't run unit tests prior to installation"
complete $install_opt -s w -l wrappers -d "Use bin wrappers for executables"
complete $install_opt -l no-wrappers -d "Don't use bin wrappers for executables"
complete $install_opt -s P -l trust-policy -d "Specify gem trust policy" -x
complete $install_opt -l ignore-dependencies -d "Do not install any required dependent gems"
complete $install_opt -s y -l include-dependencies -d "Unconditionally install the required dependent gems"

##
# list
set -l list_opt -c gem -n 'contains list (commandline -pxc)'
complete $list_opt -s d -l details -d "Display detailed information of gem(s)"
complete $list_opt -l no-details -d "Don't display detailed information of gem(s)"
complete $list_opt -s l -l local -d "Restrict operations to the LOCAL domain (default)"
complete $list_opt -s r -l remote -d "Restrict operations to the REMOTE domain"
complete $list_opt -s b -l both -d "Allow LOCAL and REMOTE operations"

##
# query
set -l query_opt -c gem -n 'contains query (commandline -pxc)'
complete $query_opt -s n -l name-matches -d "Name of gem(s) to query on matches the provided REGEXP" -x
complete $query_opt -s d -l details -d "Display detailed information of gem(s)"
complete $query_opt -l no-details -d "Don't display detailed information of gem(s)"
complete $query_opt -s l -l local -d "Restrict operations to the LOCAL domain (default)"
complete $query_opt -s r -l remote -d "Restrict operations to the REMOTE domain"
complete $query_opt -s b -l both -d "Allow LOCAL and REMOTE operations"

##
# rdoc
set -l rdoc_opt -c gem -n 'contains rdoc (commandline -pxc)'
complete $rdoc_opt -l all -d "Generate RDoc/RI documentation for all installed gems"
complete $rdoc_opt -l rdoc -d "Include RDoc generated documents"
complete $rdoc_opt -l no-rdoc -d "Don't include RDoc generated documents"
complete $rdoc_opt -l ri -d "Include RI generated documents"
complete $rdoc_opt -l no-ri -d "Don't include RI generated documents"
complete $rdoc_opt -s v -l version -d "Specify version of gem to rdoc" -x

##
# search
set -l search_opt -c gem -n 'contains search (commandline -pxc)'
complete $search_opt -s d -l details -d "Display detailed information of gem(s)"
complete $search_opt -l no-details -d "Don't display detailed information of gem(s)"
complete $search_opt -s l -l local -d "Restrict operations to the LOCAL domain (default)"
complete $search_opt -s r -l remote -d "Restrict operations to the REMOTE domain"
complete $search_opt -s b -l both -d "Allow LOCAL and REMOTE operations"

##
# specification
set -l specification_opt -c gem -n 'contains specification (commandline -pxc)'
complete $specification_opt -s v -l version -d "Specify version of gem to examine" -x
complete $specification_opt -s l -l local -d "Restrict operations to the LOCAL domain (default)"
complete $specification_opt -s r -l remote -d "Restrict operations to the REMOTE domain"
complete $specification_opt -s b -l both -d "Allow LOCAL and REMOTE operations"
complete $specification_opt -l all -d "Output specifications for all versions of the gem"

##
# uninstall
set -l uninstall_opt -c gem -n 'contains uninstall (commandline -pxc)'
complete $uninstall_opt -s a -l all -d "Uninstall all matching versions"
complete $uninstall_opt -l no-all -d "Don't uninstall all matching versions"
complete $uninstall_opt -s i -l ignore-dependencies -d "Ignore dependency requirements while uninstalling"
complete $uninstall_opt -l no-ignore-dependencies -d "Don't ignore dependency requirements while uninstalling"
complete $uninstall_opt -s x -l executables -d "Uninstall applicable executables without confirmation"
complete $uninstall_opt -l no-executables -d "Don't uninstall applicable executables without confirmation"
complete $uninstall_opt -s v -l version -d "Specify version of gem to uninstall" -x

##
# unpack
set -l unpack_opt -c gem -n 'contains unpack (commandline -pxc)'
complete $unpack_opt -s v -l version -d "Specify version of gem to unpack" -x

##
# update
set -l update_opt -c gem -n 'contains update (commandline -pxc)'
complete $update_opt -s i -l install-dir -d "Gem repository directory to get installed gems"
complete $update_opt -s N -l no-document -d "Disable documentation generation on update"
complete $update_opt -l document -a '(__fish_append , rdoc ri)' -d "Specify the documentation types you wish to generate"
complete $update_opt -s f -l force -d "Force gem to install, bypassing dependency checks"
complete $update_opt -l no-force -d "Don't force gem to install, bypassing dependency checks"
complete $update_opt -s t -l test -d "Run unit tests prior to installation"
complete $update_opt -l no-test -d "Don't run unit tests prior to installation"
complete $update_opt -s w -l wrappers -d "Use bin wrappers for executables"
complete $update_opt -l no-wrappers -d "Don't use bin wrappers for executables"
complete $update_opt -s P -l trust-policy -d "Specify gem trust policy" -x
complete $update_opt -l ignore-dependencies -d "Do not install any required dependent gems"
complete $update_opt -s y -l include-dependencies -d "Unconditionally install the required dependent gems"
complete $update_opt -l system -d "Update the RubyGems system software"
