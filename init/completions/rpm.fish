# Completions for the rpm command. Insanely complicated,
# since rpm has multiple operation modes, and a perverse number of switches.

complete -c rpm -s "?" -l help -d (_ "Display help and exit")
complete -c rpm -l version -d (_ "Display version and exit")
complete -c rpm -l quiet -d (_ "Quiet mode")
complete -c rpm -s v -d (_ "Verbose mode")
complete -c rpm -l rcfile -d (_ "List of rpm configuration files") -f
complete -c rpm -l pipe -d (_ "Pipe output through specified command") -r
complete -c rpm -l dbpath -d (_ "Specify directory for rpm database") -a "
(
	__fish_complete_directory (commandline -ct) 'Rpm database directory'
)
"
complete -c rpm -l root -d (_ "Specify root directory for rpm operations") -a "
(
	__fish_complete_directory (commandline -ct) 'Root directory for rpm operations'
)
"

set -- rpm_install -c rpm -n "__fish_contains_opt -s i -s U -s F install upgrade freshen"
complete $rpm_install -l aid -d (_ "Add suggested packages to the transaction set when needed")
complete $rpm_install -l allfiles -d (_ "Installs or upgrades all the files in the package, even if they aren't needed (missingok) and don't exist")
complete $rpm_install -l badreloc -d (_ "Used with --relocate, permit relocations on all file paths, not just those OLD-PATH's included in the binary package relocation hint(s)")
complete $rpm_install -l excludepath -d (_ "Don't install files whose name begins with specified path") -xa "(__fish_complete_directory (commandline -ct) 'Skip installation of files in this directory')"
complete $rpm_install -l excludedocs -d (_ "Don't install any files which are marked as documentation")
complete $rpm_install -l force -d (_ 'Same as using --replacepkgs, --replacefiles, and --oldpackage')
complete $rpm_install -s h -l hash -d (_ 'Print 50 hash marks as the package archive is unpacked')
complete $rpm_install -l ignoresize -d (_ "Don't check for sufficient disk space before installation")
complete $rpm_install -l ignorearch -d (_ "Allow installation or upgrading even if the architectures of the binary package and host don't match")
complete $rpm_install -l ignoreos -d (_ "Allow installation or upgrading even if the operating systems of the binary package and host don't match")
complete $rpm_install -l includedocs -d (_ 'Install documentation files (default)')
complete $rpm_install -l justdb -d (_ 'Update only the database, not the filesystem')
complete $rpm_install -l nodigest -d (_ "Don't verify package or header digests when reading")
complete $rpm_install -l nosignature -d (_ "Don't verify package or header signatures when reading")
complete $rpm_install -l nodeps -d (_ "Don't do a dependency check")
complete $rpm_install -l nosuggest -d (_ "Don't suggest package(s) that provide a missing dependency")
complete $rpm_install -l noorder -d (_ "Don't change the package installation order" )
complete $rpm_install -l noscripts -d (_ "Don't execute scripts")
complete $rpm_install -l nopre -d (_ "Don't execute pre scripts")
complete $rpm_install -l nopost -d (_ "Don't execute post scripts")
complete $rpm_install -l nopreun -d (_ "Don't execute preun scripts")
complete $rpm_install -l nopostun -d (_ "Don't execute postun scripts")
complete $rpm_install -l notriggers -d (_ "Don't execute trigger scriptlets")
complete $rpm_install -l notriggerin -d (_ "Don't execute triggerin scriptlets")
complete $rpm_install -l notriggerun -d (_ "Don't execute triggerun scriptlets")
complete $rpm_install -l notriggerpostun -d (_ "Don't execute triggerpostun scriptlets")
complete $rpm_install -l oldpackage -d (_ 'Allow an upgrade to replace a newer package with an older one')
complete $rpm_install -l percent -d (_ 'Print percentages as files are unpacked from the package archive. This is intended to make rpm easy to run from other tools')
complete $rpm_install -l prefix -d (_ 'For relocatable binary packages, translate all file paths that start with the installation prefix in the package relocation hint(s) to NEWPATH') -xa "(__fish_complete_directory (commandline -ct) 'Directory prefix for relocatable packages')"
complete $rpm_install -l relocate -x -d (_ "Translate all paths that start with first half of following parameter to second half of following parameter" )
complete $rpm_install -l repackage -d (_ 'Re-package the files before erasing')
complete $rpm_install -l replacefiles -d (_ 'Install the packages even if they replace files from other, already installed, packages')
complete $rpm_install -l replacepkgs -d (_ 'Install the packages even if some of them are already installed on this system')
complete $rpm_install -l test -d (_ "Don't install the package, simply check for and report potential conflicts")
set -e rpm_install

set -- rpm_query -c rpm -n "__fish_contains_opt -s q query"
complete $rpm_query -l changelog -d (_ 'Display change information for the package')
complete $rpm_query -s c -l configfiles -d (_ 'List only configuration files (implies -l)')
complete $rpm_query -s d -l docfiles -d (_ 'List only documentation files (implies -l)')
complete $rpm_query -l dump -d (_ 'Dump file information. Must be used with at least one of -l, -c, -d')
complete $rpm_query -l filesbypkg -d (_ 'List all the files in each selected package')
complete $rpm_query -s i -l info -d (_ 'Display package information, including name, version, and description. Uses --queryformat if specified')
complete $rpm_query -l last -d (_ 'Orders the package listing by install time')
complete $rpm_query -s l -l list -d (_ 'List files in package')
complete $rpm_query -l provides -d (_ 'List capabilities this package provides')
complete $rpm_query -s R -l requires -d (_ 'List packages on which this package depends')
complete $rpm_query -l scripts -d (_ 'List the package specific scriptlets')
complete $rpm_query -s s -l state -d (_ 'Display the states of files in the package. The state of each file is one of normal, not installed, or replaced')
complete $rpm_query -l triggers -d (_ 'Display the trigger scripts contained in the package')
complete $rpm_query -l triggerscripts -d (_ 'Display the trigger scripts contained in the package')
set -e rpm_query

set -- rpm_select -c rpm -n "__fish_contains_opt -s q -s V query verify"

complete $rpm_select -a "(__fish_print_packages)"
complete $rpm_select -s a -l all -d (_ 'Query all installed packages')
complete $rpm_select -s f -l file -d (_ 'Query package owning specified file') -r
complete $rpm_select -l fileid -d (_ 'Query package that contains a given file identifier, i.e. the MD5 digest of the file contents') -x
complete $rpm_select -s g -l group -d (_ 'Query packages with the specified group') -x
complete $rpm_select -l hdrid -d (_ 'Query package that contains a given header identifier, i.e. the SHA1 digest of the immutable header region') -x
complete $rpm_select -s p -l package -d (_ 'Query an (uninstalled) package in specified file') -xa "(__fish_complete_suffix (commandline -ct) .rpm 'Query package file')"
complete $rpm_select -l pkgid -d (_ 'Query package that contains a given package identifier, i.e. the MD5 digest of the combined header and payload contents') -x
complete $rpm_select -l specfile -d (_ 'Parse and query specified spec-file as if it were a package') -xa "(__fish_complete_suffix (commandline -ct) .spec 'Query package spec file')"
complete $rpm_select -l tid -d (_ 'Query package(s) that have the specified TID (transaction identifier)') -x
complete $rpm_select -l triggeredby -d (_ 'Query packages that are triggered by the specified packages') -x -a "(__fish_print_packages)"
complete $rpm_select -l whatprovides -d (_ 'Query all packages that provide the specified capability') -x
complete $rpm_select -l whatrequires -d (_ 'Query all packages that requires the specified capability for functioning') -x
set -e rpm_select

set -- rpm_verify -c rpm -n "__fish_contains_opt -s V verify"
complete $rpm_verify -l nodeps -d (_ "Don't verify dependencies of packages")
complete $rpm_verify -l nodigest -d (_ "Don't verify package or header digests when reading")
complete $rpm_verify -l nofiles -d (_ "Don't verify any attributes of package files")
complete $rpm_verify -l noscripts -d (_ "Don't execute the %verifyscript scriptlet")
complete $rpm_verify -l nosignature -d (_ "Don't verify package or header signatures when reading")
complete $rpm_verify -l nolinkto -d (_ "Don't verify linkto attribute")
complete $rpm_verify -l nomd5 -d (_ "Don't verify md5 attribute")
complete $rpm_verify -l nosize -d (_ "Don't verify size attribute")
complete $rpm_verify -l nouser -d (_ "Don't verify user attribute")
complete $rpm_verify -l nogroup -d (_ "Don't verify group attribute")
complete $rpm_verify -l nomtime -d (_ "Don't verify time attribute")
complete $rpm_verify -l nomode -d (_ "Don't verify mode attribute")
complete $rpm_verify -l nordev -d (_ "Don't verify dev attribute")
set -e rpm_verify

set -- rpm_erase -c rpm -n "__fish_contains_opt -s e erase"
complete $rpm_erase -a "(__fish_print_packages)"
complete $rpm_erase -l allmatches -d (_ 'Remove all versions of the package which match specified string')
complete $rpm_erase -l nodeps -d (_ "Don't check dependencies before uninstalling the packages")
complete $rpm_erase -l noscripts -d (_ "Don't execute scriplets")
complete $rpm_erase -l nopreun -d (_ "Don't execute preun scriptlet")
complete $rpm_erase -l nopostun -d (_ "Don't execute postun scriptlet")
complete $rpm_erase -l notriggers -d (_ "Don't execute trigger scriptlets")
complete $rpm_erase -l notriggerun -d (_ "Don't execute triggerun scriptlets")
complete $rpm_erase -l notriggerpostun -d (_ "Don't execute triggerpostun scriptlets")
complete $rpm_erase -l repackage -d (_ 'Re-package the files before erasing')
complete $rpm_erase -l test -d (_ "Don't really uninstall anything" )
set -e rpm_erase

set -- rpm_mode -c rpm -n '__fish_contains_opt -s e -s i -s F -s V -s U -s q erase install freshen verify upgrade query; if test $status = 0; false; else; true; end'
complete $rpm_mode -s i -l install -d (_ 'Install new package')
complete $rpm_mode -s U -l upgrade -d (_ 'Upgrade existing package')
complete $rpm_mode -s F -l freshen -d (_ 'Upgrade package if already installed')
complete $rpm_mode -s q -l query -d (_ 'Query installed packages')
complete $rpm_mode -s V -l verify -d (_ 'Verify package integrety')
complete $rpm_mode -s e -l erase -d (_ 'Erase package')
set -e rpm_mode

complete -c rpm -xa '(__fish_complete_suffix (commandline -ct) .rpm (_ "Package") )'
