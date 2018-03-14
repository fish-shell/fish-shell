# TODO unused
function __fish_equery_print_format
	printf "%s\t%s\n" \
		'$cp'           "contains the category and the package name only (e.g 'app-portage/gentoolkit')" \
		'$cpv'          "contains the category, the package name and the full version (e.g. 'app-portage/gentoolkit-0.3.0_rc10-r1')" \
		'$category'     "contains just the category (e.g. 'app-portage')" \
		'$name'         "contains just the package name (e.g. 'gentoolkit')" \
		'$version'      "contains the package version (without the revision) (e.g. '0.3.0_rc10')" \
		'$revision'     "contains the package revision (e.g. 'r1')" \
		'$fullversion'  "contains the package version including its revision (e.g. '0.3.0_rc10-r1')" \
		'$slot'         "contains the package's slot" \
		'$repo'         "contains the name of the package's repository (e.g. 'gentoo')" \
		'$mask'         "the mask-status field (~M-??)" \
		'$mask2'        "contains a verbose description of the packages masking status" \
		'$location'     "the location field (IPO-)"
end

## Global Opts
complete -c equery -s h -l help         -d "Displays a help message"
complete -c equery -s q -l quiet        -d "Minimal output"
complete -c equery -s C -l no-color     -d "Turns off colors"
complete -c equery -s N -l no-pipe      -d "Turns off pipe detection"
complete -c equery -s V -l version      -d "Display version information"

## Subcommands
complete -c equery -n '__fish_use_subcommand' -xa 'belongs'     -d "List all packages owning file(s)"
complete -c equery -n '__fish_use_subcommand' -xa 'changes'     -d "List changelog entries for ATOM"
complete -c equery -n '__fish_use_subcommand' -xa 'check'       -d "Check MD5sums and timestamps of package"
complete -c equery -n '__fish_use_subcommand' -xa 'depends'     -d "List all packages depending on specified package"
complete -c equery -n '__fish_use_subcommand' -xa 'depgraph'    -d "Display a dependency tree for package"
complete -c equery -n '__fish_use_subcommand' -xa 'files'       -d "List files owned by package"
complete -c equery -n '__fish_use_subcommand' -xa 'has'         -d "List pkgs for matching ENVIRONMENT data"
complete -c equery -n '__fish_use_subcommand' -xa 'hasuse'      -d "List pkgs with specified useflag"
complete -c equery -n '__fish_use_subcommand' -xa 'keywords'    -d "Display keywords for specified PKG"
complete -c equery -n '__fish_use_subcommand' -xa 'list'        -d "List all packages matching pattern"
complete -c equery -n '__fish_use_subcommand' -xa 'meta'        -d "Display metadata about PKG"
complete -c equery -n '__fish_use_subcommand' -xa 'size'        -d "Print size of files contained in package"
complete -c equery -n '__fish_use_subcommand' -xa 'uses'        -d "Display USE flags for package"
complete -c equery -n '__fish_use_subcommand' -xa 'which'       -d "Print full path to ebuild for package"

## Arguments
complete -c equery -n '__fish_seen_subcommand_from changes depends depgraph meta which' -xa '(__fish_portage_print_available_pkgs)'
complete -c equery -n '__fish_seen_subcommand_from check files uses size' -xa '(__fish_portage_print_installed_pkgs)'

## Local opts
# belongs
complete -c equery -n '__fish_seen_subcommand_from belongs' -s f -l full-regex        -d "Supplied query is a regex"
complete -c equery -n '__fish_seen_subcommand_from belongs' -s e -l early-out         -d "Stop when first match is found"
complete -c equery -n '__fish_seen_subcommand_from belongs' -s n -l name-only         -d "Don't print the version"

# changes
complete -c equery -n '__fish_seen_subcommand_from changes' -s l -l latest            -d "Display only the latest ChangeLog entry"
complete -c equery -n '__fish_seen_subcommand_from changes' -s f -l full              -d "Display the full ChangeLog"
#complete -c equery -n '__fish_seen_subcommand_from changes'      -l limit=NUM         -d "Limit the number of entries displayed (with --full)"
#complete -c equery -n '__fish_seen_subcommand_from changes'      -l from=VER          -d "Set which version to display from"
#complete -c equery -n '__fish_seen_subcommand_from changes'      -l to=VER            -d "Set which version to display to"

# check
complete -c equery -n '__fish_seen_subcommand_from check' -s f -l full-regex        -d "Query is a regular expression"
complete -c equery -n '__fish_seen_subcommand_from check' -s o -l only-failures     -d "Only display packages that do not pass"

# depends
complete -c equery -n '__fish_seen_subcommand_from depends' -s a -l all-packages      -d "Include dependencies that are not installed (slow)"
complete -c equery -n '__fish_seen_subcommand_from depends' -s D -l indirect          -d "Search both direct and indirect dependencies"
#complete -c equery -n '__fish_seen_subcommand_from depends'      -l depth=N           -d "Limit indirect dependency tree to specified depth"

# depgraph
complete -c equery -n '__fish_seen_subcommand_from depgraph' -s A -l no-atom           -d "Do not show dependency atom"
complete -c equery -n '__fish_seen_subcommand_from depgraph' -s M -l no-mask           -d "Do not show masking status"
complete -c equery -n '__fish_seen_subcommand_from depgraph' -s U -l no-useflags       -d "Do not show USE flags"
complete -c equery -n '__fish_seen_subcommand_from depgraph' -s l -l linear            -d "Do not format the graph by indenting dependencies"
#complete -c equery -n '__fish_seen_subcommand_from depgraph'      -l depth=N           -d "Limit dependency graph to specified depth"

# files
complete -c equery -n '__fish_seen_subcommand_from files' -s m -l md5sum            -d "Include MD5 sum in output"
complete -c equery -n '__fish_seen_subcommand_from files' -s s -l timestamp         -d "Include timestamp in output"
complete -c equery -n '__fish_seen_subcommand_from files' -s t -l type              -d "Include file type in output"
complete -c equery -n '__fish_seen_subcommand_from files'      -l tree              -d "Display results in a tree (turns off other options)"
# TODO comma separated list
complete -c equery -n '__fish_seen_subcommand_from files' -s f -l filter            -d "Filter output by file type (comma separated list)" \
	-xa "dir obj sym dev fifo path conf cmd doc man info"

# has + hasuse
complete -c equery -n '__fish_seen_subcommand_from has hasuse' -s I -l exclude-installed -d "Exclude installed packages from search path"
complete -c equery -n '__fish_seen_subcommand_from has hasuse' -s o -l overlay-tree      -d "Include overlays in search path"
complete -c equery -n '__fish_seen_subcommand_from has hasuse' -s p -l portage-tree      -d "Include entire portage tree in search path"
#complete -c equery -n '__fish_seen_subcommand_from has hasuse' -s F -l format=TMPL       -d "Specify a custom output format"

# keywords
# TODO

# list
complete -c equery -n '__fish_seen_subcommand_from list' -s d -l duplicates        -d "List only installed duplicate packages"
complete -c equery -n '__fish_seen_subcommand_from list' -s b -l binpkgs-missing   -d "List only installed packages without a corresponding binary package"
complete -c equery -n '__fish_seen_subcommand_from list' -s f -l full-regex        -d "Query is a regular expression"
complete -c equery -n '__fish_seen_subcommand_from list' -s m -l mask-reason       -d "Include reason for package mask"
complete -c equery -n '__fish_seen_subcommand_from list' -s I -l exclude-installed -d "Exclude installed packages from output"
complete -c equery -n '__fish_seen_subcommand_from list' -s o -l overlay-tree      -d "List packages in overlays"
complete -c equery -n '__fish_seen_subcommand_from list' -s p -l portage-tree      -d "List packages in the main portage tree"
#complete -c equery -n '__fish_seen_subcommand_from list' -s F -l format=TMPL       -d "Specify a custom output format"

# meta
complete -c equery -n '__fish_seen_subcommand_from meta' -s d -l description       -d "Show an extended package description"
complete -c equery -n '__fish_seen_subcommand_from meta' -s H -l herd              -d "Show the herd(s) for the package"
complete -c equery -n '__fish_seen_subcommand_from meta' -s k -l keywords          -d "Show keywords for all matching package versions"
complete -c equery -n '__fish_seen_subcommand_from meta' -s l -l license           -d "Show licenses for the best maching version"
complete -c equery -n '__fish_seen_subcommand_from meta' -s m -l maintainer        -d "Show the maintainer(s) for the package"
complete -c equery -n '__fish_seen_subcommand_from meta' -s S -l stablreq          -d "Show STABLEREQ arches (cc's) for all matching package versions"
complete -c equery -n '__fish_seen_subcommand_from meta' -s u -l useflags          -d "Show per-package USE flag descriptions"
complete -c equery -n '__fish_seen_subcommand_from meta' -s U -l upstream          -d "Show package's upstream information"
complete -c equery -n '__fish_seen_subcommand_from meta' -s x -l xml               -d "Show the plain metadata.xml file"

# size
complete -c equery -n '__fish_seen_subcommand_from size' -s b -l bytes             -d "Report size in bytes"
complete -c equery -n '__fish_seen_subcommand_from size' -s f -l full-regex        -d "Query is a regular expression"

# uses
complete -c equery -n '__fish_seen_subcommand_from uses' -s a -l all               -d "Include all package versions"
complete -c equery -n '__fish_seen_subcommand_from uses' -s i -l ignore-l10n       -d "Don't show l10n USE flags"

# which
complete -c equery -n '__fish_seen_subcommand_from which' -s m -l include-masked    -d "Return highest version ebuild available"
complete -c equery -n '__fish_seen_subcommand_from which' -s e -l ebuild            -d "Print the ebuild"
