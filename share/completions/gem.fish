# Completions for rubygem's `gem' command

# Author: Tassilo Horn <tassilo@member.fsf.org>

#####
# Global options
complete -c gem -n 'not __subcommand_given' -s h -l help -d "Print usage informations and quit"
complete -c gem -n 'not __subcommand_given' -s v -l version -d "Print the version and quit"

#####
# Subcommands
complete -c gem -n '__fish_use_subcommand' -xa 'build\t"'(_ "Build a gem from a gemspec")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'cert\t"'(_ "Adjust RubyGems certificate settings")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'check\t"'(_ "Check installed gems")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'cleanup\t"'(_ "Cleanup old versions of installed gems in the local repository")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'contents\t"'(_ "Display the contents of the installed gems")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'dependency\t"'(_ "Show the dependencies of an installed gem")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'environment\t"'(_ "Display RubyGems environmental information")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'help\t"'(_ "Provide help on the 'gem' command")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'install\t"'(_ "Install a gem into the local repository")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'list\t"'(_ "Display all gems whose name starts with STRING")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'query\t"'(_ "Query gem information in local or remote repositories")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'rdoc\t"'(_ "Generates RDoc for pre-installed gems")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'search\t"'(_ "Display all gems whose name contains STRING")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'specification\t"'(_ "Display gem specification (in yaml)")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'uninstall\t"'(_ "Uninstall a gem from the local repository")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'unpack\t"'(_ "Unpack an installed gem to the current directory")'"'
complete -c gem -n '__fish_use_subcommand' -xa 'update\t"'(_ "Update the named gem (or all installed gems) in the local repository")'"'

#####
# Subcommand switches

# common opts
set -l common_opt -c gem -n 'not __fish_use_subcommand' #'__subcommand_given'
complete $common_opt -l source -d (N_ "Use URL as the remote source for gems") -x
complete $common_opt -s p -l http-proxy -d (N_ "Use the given HTTP proxy for remote operations") -x
complete $common_opt -l no-http-proxy -d (N_ "Use no HTTP proxy for remote operations")
complete $common_opt -s h -l help -d (N_ "Get help on this command")
complete $common_opt -s v -l verbose -d (N_ "Set the verbose level of output")
complete $common_opt -l config-file -d (N_ "Use this config file instead of default") -x
complete $common_opt -l backtrace -d (N_ "Show stack backtrace on errors")
complete $common_opt -l debug -d (N_ "Turn on Ruby debugging")

##
# cert
set -l cert_opt -c gem -n 'contains cert (commandline -poc)'
complete $cert_opt -s a -l add -d (N_ "Add a trusted certificate") -x
complete $cert_opt -s l -l list -d (N_ "List trusted certificates")
complete $cert_opt -s r -l remove -d (N_ "Remove trusted certificates containing STRING") -x
complete $cert_opt -s b -l build -d (N_ "Build private key and self-signed certificate for EMAIL_ADDR") -x
complete $cert_opt -s C -l certificate -d (N_ "Certificate for --sign command") -x
complete $cert_opt -s K -l private-key -d (N_ "Private key for --sign command") -x
complete $cert_opt -s s -l sign -d (N_ "Sign a certificate with my key and certificate") -x

##
# check
set -l check_opt -c gem -n 'contains check (commandline -poc)'
complete $check_opt -s v -l verify -d (N_ "Verify gem file against its internal checksum") -x
complete $check_opt -s a -l alien -d (N_ "Report 'unmanaged' or rogue files in the gem repository")
complete $check_opt -s t -l test -d (N_ "Run unit tests for gem")
complete $check_opt -s V -l version -d (N_ "Specify version for which to run unit tests")

##
# cleanup
set -l cleanup_opt -c gem -n 'contains cleanup (commandline -poc)'
complete $cleanup_opt -s d -l dryrun -d (N_ "Don't really cleanup")

##
# contents
set -l contents_opt -c gem -n 'contains contents (commandline -poc)'
complete $contents_opt -s l -l list -d (N_ "List the files inside a Gem")
complete $contents_opt -s V -l version -d (N_ "Specify version for gem to view")
complete $contents_opt -s s -l spec-dir -d (N_ "Search for gems under specific paths") -x
complete $contents_opt -s v -l verbose -d (N_ "Be verbose when showing status")

##
# dependency
set -l dep_opt -c gem -n 'contains dependency (commandline -poc)'
complete $dep_opt -s v -l version -d (N_ "Specify version of gem to uninstall") -x
complete $dep_opt -s r -l reverse-dependencies -d (N_ "Include reverse dependencies in the output")
complete $dep_opt -l no-reverse-dependencies -d (N_ "Don't include reverse dependencies in the output")
complete $dep_opt -s p -l pipe -d (N_ "Pipe Format (name --version ver)")

##
# environment
set -l env_opt -c gem -n 'contains environment (commandline -poc)'
complete $env_opt -xa 'packageversion\t"'(_ "display the package version")'" gemdir\t"'(_ "display the path where gems are installed")'" gempath\t"'(_ "display path used to search for gems")'" version\t"'(_ "display the gem format version")'" remotesources\t"'(_ "display the remote gem servers")'"'

##
# help
set -l help_opt -c gem -n 'contains help (commandline -poc)'
complete $help_opt -xa 'commands\t"'(_ "list all 'gem' commands")'" examples\t"'(_ "show some examples of usage")'" build cert check cleanup contents dependency environment help install list query rdoc search specification uninstall unpack update'

##
# install
set -l install_opt -c gem -n 'contains install (commandline -poc)'
complete $install_opt -s v -l version -d (N_ "Specify version of gem to install") -x
complete $install_opt -s l -l local -d (N_ "Restrict operations to the LOCAL domain (default)")
complete $install_opt -s r -l remote -d (N_ "Restrict operations to the REMOTE domain")
complete $install_opt -s b -l both -d (N_ "Allow LOCAL and REMOTE operations")
complete $install_opt -s i -l install-dir -d (N_ "Gem repository directory to get installed gems") -x
complete $install_opt -s d -l rdoc -d (N_ "Generate RDoc documentation for the gem on install")
complete $install_opt -l no-rdoc -d (N_ "Don't generate RDoc documentation for the gem on install")
complete $install_opt -l ri -d (N_ "Generate RI documentation for the gem on install")
complete $install_opt -l no-ri -d (N_ "Don't generate RI documentation for the gem on install")
complete $install_opt -s f -l force -d (N_ "Force gem to install, bypassing dependency checks")
complete $install_opt -l no-force -d (N_ "Don't force gem to install, bypassing dependency checks")
complete $install_opt -s t -l test -d (N_ "Run unit tests prior to installation")
complete $install_opt -l no-test -d (N_ "Don't run unit tests prior to installation")
complete $install_opt -s w -l wrappers -d (N_ "Use bin wrappers for executables")
complete $install_opt -l no-wrappers -d (N_ "Don't use bin wrappers for executables")
complete $install_opt -s P -l trust-policy -d (N_ "Specify gem trust policy") -x
complete $install_opt -l ignore-dependencies -d (N_ "Do not install any required dependent gems")
complete $install_opt -s y -l include-dependencies -d (N_ "Unconditionally install the required dependent gems")

##
# list
set -l list_opt -c gem -n 'contains list (commandline -poc)'
complete $list_opt -s d -l details -d (N_ "Display detailed information of gem(s)")
complete $list_opt -l no-details -d (N_ "Don't display detailed information of gem(s)")
complete $list_opt -s l -l local -d (N_ "Restrict operations to the LOCAL domain (default)")
complete $list_opt -s r -l remote -d (N_ "Restrict operations to the REMOTE domain")
complete $list_opt -s b -l both -d (N_ "Allow LOCAL and REMOTE operations")

##
# query
set -l query_opt -c gem -n 'contains query (commandline -poc)'
complete $query_opt -s n -l name-matches -d (N_ "Name of gem(s) to query on matches the provided REGEXP") -x
complete $query_opt -s d -l details -d (N_ "Display detailed information of gem(s)")
complete $query_opt -l no-details -d (N_ "Don't display detailed information of gem(s)")
complete $query_opt -s l -l local -d (N_ "Restrict operations to the LOCAL domain (default)")
complete $query_opt -s r -l remote -d (N_ "Restrict operations to the REMOTE domain")
complete $query_opt -s b -l both -d (N_ "Allow LOCAL and REMOTE operations")

##
# rdoc
set -l rdoc_opt -c gem -n 'contains rdoc (commandline -poc)'
complete $rdoc_opt -l all -d (N_ "Generate RDoc/RI documentation for all installed gems")
complete $rdoc_opt -l rdoc -d (N_ "Include RDoc generated documents")
complete $rdoc_opt -l no-rdoc -d (N_ "Don't include RDoc generated documents")
complete $rdoc_opt -l ri -d (N_ "Include RI generated documents")
complete $rdoc_opt -l no-ri -d (N_ "Don't include RI generated documents")
complete $rdoc_opt -s v -l version -d (N_ "Specify version of gem to rdoc") -x

##
# search
set -l search_opt -c gem -n 'contains search (commandline -poc)'
complete $search_opt -s d -l details -d (N_ "Display detailed information of gem(s)")
complete $search_opt -l no-details -d (N_ "Don't display detailed information of gem(s)")
complete $search_opt -s l -l local -d (N_ "Restrict operations to the LOCAL domain (default)")
complete $search_opt -s r -l remote -d (N_ "Restrict operations to the REMOTE domain")
complete $search_opt -s b -l both -d (N_ "Allow LOCAL and REMOTE operations")

##
# specification
set -l specification_opt -c gem -n 'contains specification (commandline -poc)'
complete $specification_opt -s v -l version -d (N_ "Specify version of gem to examine") -x
complete $specification_opt -s l -l local -d (N_ "Restrict operations to the LOCAL domain (default)")
complete $specification_opt -s r -l remote -d (N_ "Restrict operations to the REMOTE domain")
complete $specification_opt -s b -l both -d (N_ "Allow LOCAL and REMOTE operations")
complete $specification_opt -l all -d (N_ "Output specifications for all versions of the gem")

##
# uninstall
set -l uninstall_opt -c gem -n 'contains uninstall (commandline -poc)'
complete $uninstall_opt -s a -l all -d (N_ "Uninstall all matching versions")
complete $uninstall_opt -l no-all -d (N_ "Don't uninstall all matching versions")
complete $uninstall_opt -s i -l ignore-dependencies -d (N_ "Ignore dependency requirements while uninstalling")
complete $uninstall_opt -l no-ignore-dependencies -d (N_ "Don't ignore dependency requirements while uninstalling")
complete $uninstall_opt -s x -l executables -d (N_ "Uninstall applicable executables without confirmation")
complete $uninstall_opt -l no-executables -d (N_ "Don't uninstall applicable executables without confirmation")
complete $uninstall_opt -s v -l version -d (N_ "Specify version of gem to uninstall") -x

##
# unpack
set -l unpack_opt -c gem -n 'contains unpack (commandline -poc)'
complete $unpack_opt -s v -l version -d (N_ "Specify version of gem to unpack") -x

##
# update
set -l update_opt -c gem -n 'contains update (commandline -poc)'
complete $update_opt -s i -l install-dir -d (N_ "Gem repository directory to get installed gems")
complete $update_opt -s d -l rdoc -d (N_ "Generate RDoc documentation for the gem on install")
complete $update_opt -l no-rdoc -d (N_ "Don't generate RDoc documentation for the gem on install")
complete $update_opt -l ri -d (N_ "Generate RI documentation for the gem on install")
complete $update_opt -l no-ri -d (N_ "Don't generate RI documentation for the gem on install")
complete $update_opt -s f -l force -d (N_ "Force gem to install, bypassing dependency checks")
complete $update_opt -l no-force -d (N_ "Don't force gem to install, bypassing dependency checks")
complete $update_opt -s t -l test -d (N_ "Run unit tests prior to installation")
complete $update_opt -l no-test -d (N_ "Don't run unit tests prior to installation")
complete $update_opt -s w -l wrappers -d (N_ "Use bin wrappers for executables")
complete $update_opt -l no-wrappers -d (N_ "Don't use bin wrappers for executables")
complete $update_opt -s P -l trust-policy -d (N_ "Specify gem trust policy") -x
complete $update_opt -l ignore-dependencies -d (N_ "Do not install any required dependent gems")
complete $update_opt -s y -l include-dependencies -d (N_ "Unconditionally install the required dependent gems")
complete $update_opt -l system -d (N_ "Update the RubyGems system software")

