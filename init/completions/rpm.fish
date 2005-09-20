# Completions for the rpm command. Insanely complicated,
# since rpm has multiple operation modes, and a perverse number of switches.

complete -c rpm -s "?" -l help -d "Print help and exit"
complete -c rpm -l version -d "Print version and exit"
complete -c rpm -l quiet -d "Be less verbose"
complete -c rpm -s v -d "Be more verbose"
complete -c rpm -l rcfile -d "List of rpm configuration files" -f
complete -c rpm -l pipe -d "Pipe output through specified command" -r
complete -c rpm -l dbpath -d "Specify directory for rpm database" -a "
(
	__fish_complete_directory (commandline -ct) 'Rpm database directory'
)
"
complete -c rpm -l root -d "Specify root directory for rpm operations" -a "
(
	__fish_complete_directory (commandline -ct) 'Root directory for rpm operations'
)
"

set -- rpm_install -c rpm -n "__fish_contains_opt -s i -s U -s F install upgrade freshen"
complete $rpm_install -l aid -d "Add suggested packages to the transaction set when needed"
complete $rpm_install -l allfiles -d "Installs or upgrades all the missing ok files in the package, regardless if they exist"
complete $rpm_install -l badreloc -d "Used with --relocate, permit relocations on all file paths, not just those OLD-PATH’s included in the binary package relocation hint(s)"
complete $rpm_install -l aid -d 'Add suggested packages to the transaction set when needed'
complete $rpm_install -l allfiles -d 'Installs or upgrades all the missingok files in the package, regardless if they exist'
complete $rpm_install -l badreloc -d 'Used with --relocate, permit relocations on all file paths, not just those OLD-PATH’s included in the binary package relocation hint(s)'
complete $rpm_install -l excludepath -d 'Dont install files whose name begins with OLDPATH' -xa "(__fish_complete_directory (commandline -ct) 'Skip installation of files in this directory')"
complete $rpm_install -l excludedocs -d 'Dont install any files which are marked as documentation (which includes man pages and texinfo documents)'
complete $rpm_install -l force -d 'Same as using --replacepkgs, --replacefiles, and --oldpackage'
complete $rpm_install -s h -l hash -d 'Print 50 hash marks as the package archive is unpacked. Use with -v or --verbose for a nicer display'
complete $rpm_install -l ignoresize -d 'Dont check mount file systems for sufficient disk space before installing this package'
complete $rpm_install -l ignorearch -d 'Allow installation or upgrading even if the architectures of the binary package and host dont match'
complete $rpm_install -l ignoreos -d 'Allow installation or upgrading even if the operating systems of the binary package and host dont match'
complete $rpm_install -l includedocs -d 'Install documentation files. This is the default behavior'
complete $rpm_install -l justdb -d 'Update only the database, not the filesystem'
complete $rpm_install -l nodigest -d 'Dont verify package or header digests when reading'
complete $rpm_install -l nosignature -d 'Dont verify package or header signatures when reading'
complete $rpm_install -l nodeps -d 'Dont do a dependency check before installing or upgrading a package'
complete $rpm_install -l nosuggest -d 'Dont suggest package(s) that provide a missing dependency'
complete $rpm_install -l noorder -d 'Dont reorder the packages for an install. The list of packages would normally be reordered to satisfy dependencies'
complete $rpm_install -l noscripts -d 'Dont execute scripts'
complete $rpm_install -l nopre -d 'Dont execute pre scripts'
complete $rpm_install -l nopost -d 'Dont execute post scripts'
complete $rpm_install -l nopreun -d 'Dont execute preun scripts'
complete $rpm_install -l nopostun -d 'Dont execute postun scripts'
complete $rpm_install -l notriggers -d 'Dont execute trigger scriptlets'
complete $rpm_install -l notriggerin -d 'Dont execute triggerin scriptlets'
complete $rpm_install -l notriggerun -d 'Dont execute triggerun scriptlets'
complete $rpm_install -l notriggerpostun -d 'Dont execute triggerpostun scriptlets'
complete $rpm_install -l oldpackage -d 'Allow an upgrade to replace a newer package with an older one'
complete $rpm_install -l percent -d 'Print percentages as files are unpacked from the package archive. This is intended to make rpm easy to run from other tools'
complete $rpm_install -l prefix -d 'For relocatable binary packages, translate all file paths that start with the installation prefix in the package relocation hint(s) to NEWPATH' -xa "(__fish_complete_directory (commandline -ct) 'Directory prefix for relocatable packages')"
complete $rpm_install -l relocate -x -d 'For relocatable binary packages, translate all file paths that start with OLDPATH in the package relocation hint(s) to NEWPATH. This option can be used repeatedly if several OLDPATH’s in the package are to be relocated'
complete $rpm_install -l repackage -d 'Re-package the files before erasing. The previously installed package will be named according to the macro %_repackage_name_fmt and will be created in the directory named by the macro %_repackage_dir (default value is /var/spool/repackage)'
complete $rpm_install -l replacefiles -d 'Install the packages even if they replace files from other, already installed, packages'
complete $rpm_install -l replacepkgs -d 'Install the packages even if some of them are already installed on this system'
complete $rpm_install -l test -d 'Do not install the package, simply check for and report potential conflicts'
set -e rpm_install

set -- rpm_query -c rpm -n "__fish_contains_opt -s q query"
complete $rpm_query -l changelog -d 'Display change information for the package'
complete $rpm_query -s c -l configfiles -d 'List only configuration files (implies -l)'
complete $rpm_query -s d -l docfiles -d 'List only documentation files (implies -l)'
complete $rpm_query -l dump -d 'Dump file information. Must be used with at least one of -l, -c, -d'
complete $rpm_query -l filesbypkg -d 'List all the files in each selected package'
complete $rpm_query -s i -l info -d 'Display package information, including name, version, and description. This uses the --queryformat if one was specified'
complete $rpm_query -l last -d 'Orders the package listing by install time such that the latest packages are at the top'
complete $rpm_query -s l -l list -d 'List files in package'
complete $rpm_query -l provides -d 'List capabilities this package provides'
complete $rpm_query -s R -l requires -d 'List packages on which this package depends'
complete $rpm_query -l scripts -d 'List the package specific scriptlet(s) that are used as part of the installation and uninstallation processes'
complete $rpm_query -s s -l state -d 'Display the states of files in the package (implies -l). The state of each file is one of normal, not installed, or replaced'
complete $rpm_query -l triggers -d 'Display the trigger scripts, if any, which are contained in the package'
complete $rpm_query -l triggerscripts -d 'Display the trigger scripts, if any, which are contained in the package'
set -e rpm_query

set -- rpm_select -c rpm -n "__fish_contains_opt -s q -s V query verify"

complete $rpm_select -a "(__fish_print_packages)"
complete $rpm_select -s a -l all -d 'Query all installed packages'
complete $rpm_select -s f -l file -d 'Query package owning FILE' -r
complete $rpm_select -l fileid -d 'Query package that contains a given file identifier, i.e. the MD5 digest of the file contents' -x
complete $rpm_select -s g -l group -d 'Query packages with the group of GROUP' -x
complete $rpm_select -l hdrid -d 'Query package that contains a given header identifier, i.e. the SHA1 digest of the immutable header region' -x
complete $rpm_select -s p -l package -d 'Query an (uninstalled) package PACKAGE_FILE' -xa "(__fish_complete_suffix (commandline -ct) .rpm 'Query package file')"
complete $rpm_select -l pkgid -d 'Query package that contains a given package identifier, i.e. the MD5 digest of the combined header and payload contents' -x
complete $rpm_select -l specfile -d 'Parse and query SPECFILE as if it were a package' -xa "(__fish_complete_suffix (commandline -ct) .spec 'Query package spec file')"
complete $rpm_select -l tid -d 'Query package(s) that have a given TID transaction identifier' -x
complete $rpm_select -l triggeredby -d 'Query packages that are triggered by package(s) PACKAGE_NAME' -x -a "(__fish_print_packages)"
complete $rpm_select -l whatprovides -d 'Query all packages that provide the CAPABILITY capability' -x
complete $rpm_select -l whatrequires -d 'Query all packages that requires CAPABILITY for proper functioning' -x
set -e rpm_select

set -- rpm_verify -c rpm -n "__fish_contains_opt -s V verify"
complete $rpm_verify -l nodeps -d 'Dont verify dependencies of packages'
complete $rpm_verify -l nodigest -d 'Dont verify package or header digests when reading'
complete $rpm_verify -l nofiles -d 'Dont verify any attributes of package files'
complete $rpm_verify -l noscripts -d 'Dont execute the %verifyscript scriptlet (if any)'
complete $rpm_verify -l nosignature -d 'Don’t verify package or header signatures when reading'
complete $rpm_verify -l nolinkto -d 'Dont verify linkto attribute'
complete $rpm_verify -l nomd5 -d 'Dont verify md5 attribute'
complete $rpm_verify -l nosize -d 'Dont verify size attribute'
complete $rpm_verify -l nouser -d 'Dont verify user attribute'
complete $rpm_verify -l nogroup -d 'Dont verify group attribute'
complete $rpm_verify -l nomtime -d 'Dont verify time attribute'
complete $rpm_verify -l nomode -d 'Dont verify mode attribute'
complete $rpm_verify -l nordev -d 'Dont verify dev attribute'
set -e rpm_verify

set -- rpm_erase -c rpm -n "__fish_contains_opt -s e erase"
complete $rpm_erase -a "(__fish_print_packages)"
complete $rpm_erase -l allmatches -d 'Remove all versions of the package which match PACKAGE_NAME. Normally an error is issued if PACKAGE_NAME matches multiple packages'
complete $rpm_erase -l nodeps -d 'Don’t check dependencies before uninstalling the packages'
complete $rpm_erase -l noscripts -d 'Dont execute scriplets'
complete $rpm_erase -l nopreun -d 'Dont execute preun scriptlet'
complete $rpm_erase -l nopostun -d 'Dont execute postun scriptlet'
complete $rpm_erase -l notriggers -d 'Dont execute trigger scriptlets'
complete $rpm_erase -l notriggerun -d 'Dont execute triggerun scriptlets'
complete $rpm_erase -l notriggerpostun -d 'Dont execute triggerpostun scriptlets'
complete $rpm_erase -l repackage -d 'Re-package the files before erasing'
complete $rpm_erase -l test -d 'Dont really uninstall anything, just go through the motions'
set -e rpm_erase

set -- rpm_mode -c rpm -n "__fish_contains_opt -s e -s i -s F -s V -s U -s q erase install freshen verify upgrade query; if test $status = 0; false; else; true; end"
complete $rpm_mode -s i -l install -d 'Install new package'
complete $rpm_mode -s U -l upgrade -d 'Upgrade existing package'
complete $rpm_mode -s F -l freshen -d 'Upgrade package if already installed'
complete $rpm_mode -s q -l query -d 'Query installed packages'
complete $rpm_mode -s V -l verify -d 'Verify package integrety'
complete $rpm_mode -s e -l erase -d 'Erase package'
set -e rpm_mode

complete -c rpm -xa '(__fish_complete_suffix (commandline -ct) .rpm "RPM package")'

