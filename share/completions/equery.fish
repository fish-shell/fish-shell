# TODO unused
# function __fish_equery_print_format
#     printf "%s\t%s\n" \
#         '$cp' "Category and package name (e.g 'app-portage/gentoolkit')" \
#         '$cpv' "Category, package name and version (e.g. 'app-portage/gentoolkit-0.3.0_rc10-r1')" \
#         '$category' "Category (e.g. 'app-portage')" \
#         '$name' "Package name (e.g. 'gentoolkit')" \
#         '$version' "Version (without the revision) (e.g. '0.3.0_rc10')" \
#         '$revision' "Revision (e.g. 'r1')" \
#         '$fullversion' "Version including revision (e.g. '0.3.0_rc10-r1')" \
#         '$slot' Slot \
#         '$repo' "Repository (e.g. 'gentoo')" \
#         '$mask' "Mask-status field (~M-??)" \
#         '$mask2' "Verbose description of the masking status" \
#         '$location' "Location field (IPO-)"
# end

## Global Opts
complete -c equery -s h -l help -d "Displays a help message"
complete -c equery -s q -l quiet -d "Minimal output"
complete -c equery -s C -l no-color -d "Turns off colors"
complete -c equery -s N -l no-pipe -d "Turns off pipe detection"
complete -c equery -s V -l version -d "Display version information"

## Subcommands
complete -c equery -n __fish_use_subcommand -xa belongs -d "List all pkgs owning file(s)"
complete -c equery -n __fish_use_subcommand -xa changes -d "List changelog entries for ATOM"
complete -c equery -n __fish_use_subcommand -xa check -d "Check pkg's MD5sums and timestamps"
complete -c equery -n __fish_use_subcommand -xa depends -d "List all pkgs depending on specified pkg"
complete -c equery -n __fish_use_subcommand -xa depgraph -d "Display pkg's dependency tree"
complete -c equery -n __fish_use_subcommand -xa files -d "List files owned by pkg"
complete -c equery -n __fish_use_subcommand -xa has -d "List pkgs for matching ENVIRONMENT data"
complete -c equery -n __fish_use_subcommand -xa hasuse -d "List pkgs with specified useflag"
complete -c equery -n __fish_use_subcommand -xa keywords -d "Display pkg's keywords"
complete -c equery -n __fish_use_subcommand -xa list -d "List all pkgs matching pattern"
complete -c equery -n __fish_use_subcommand -xa meta -d "Display pkg's metadata"
complete -c equery -n __fish_use_subcommand -xa size -d "Print size of files contained in pkg"
complete -c equery -n __fish_use_subcommand -xa uses -d "Display pkg's USE flags"
complete -c equery -n __fish_use_subcommand -xa which -d "Print full path to ebuild for pkg"

## Arguments
complete -c equery -n '__fish_seen_subcommand_from c changes d depends g depgraph y keywords m meta u uses w which' \
    -xa '(__fish_print_portage_available_pkgs)'
complete -c equery -n '__fish_seen_subcommand_from k check f files s size' \
    -xa '(__fish_print_portage_installed_pkgs)'

## Local opts
# belongs
complete -c equery -n '__fish_seen_subcommand_from b belongs' -s f -l full-regex -d "Query is a regex"
complete -c equery -n '__fish_seen_subcommand_from b belongs' -s e -l early-out -d "Stop after first match"
complete -c equery -n '__fish_seen_subcommand_from b belongs' -s n -l name-only -d "Omit version"

# changes
complete -c equery -n '__fish_seen_subcommand_from c changes' -s l -l latest -d "Display only latest ChangeLog entry"
complete -c equery -n '__fish_seen_subcommand_from c changes' -s f -l full -d "Display full ChangeLog"
complete -c equery -n '__fish_seen_subcommand_from c changes' -l limit -d "Limit number of entries displayed (with --full)" \
    -xa "(seq 99)"
#complete -c equery -n '__fish_seen_subcommand_from c changes'      -l from=VER          -d "Set which version to display from"
#complete -c equery -n '__fish_seen_subcommand_from c changes'      -l to=VER            -d "Set which version to display to"

# check
complete -c equery -n '__fish_seen_subcommand_from k check' -s f -l full-regex -d "Query is a regex"
complete -c equery -n '__fish_seen_subcommand_from k check' -s o -l only-failures -d "Only display pkgs that do not pass"

# depends
complete -c equery -n '__fish_seen_subcommand_from d depends' -s a -l all-packages -d "Include dependencies that are not installed (slow)"
complete -c equery -n '__fish_seen_subcommand_from d depends' -s D -l indirect -d "Search both direct and indirect dependencies"
complete -c equery -n '__fish_seen_subcommand_from d depends' -l depth -d "Limit indirect dependency tree to specified depth" \
    -xa "(seq 9)"

# depgraph
complete -c equery -n '__fish_seen_subcommand_from g depgraph' -s A -l no-atom -d "Don't show dependency atom"
complete -c equery -n '__fish_seen_subcommand_from g depgraph' -s M -l no-mask -d "Don't show masking status"
complete -c equery -n '__fish_seen_subcommand_from g depgraph' -s U -l no-useflags -d "Don't show USE flags"
complete -c equery -n '__fish_seen_subcommand_from g depgraph' -s l -l linear -d "Don't indent dependencies"
complete -c equery -n '__fish_seen_subcommand_from g depgraph' -l depth -d "Limit dependency graph to specified depth" \
    -xa "(seq 9)"

# files
function __fish_equery_files_filter_args
    printf "%s\n" dir obj sym dev fifo path conf cmd doc man info
end
complete -c equery -n '__fish_seen_subcommand_from f files' -s m -l md5sum -d "Include MD5 sum in output"
complete -c equery -n '__fish_seen_subcommand_from f files' -s s -l timestamp -d "Include timestamp in output"
complete -c equery -n '__fish_seen_subcommand_from f files' -s t -l type -d "Include file type in output"
complete -c equery -n '__fish_seen_subcommand_from f files' -l tree -d "Display results in a tree"
complete -c equery -n '__fish_seen_subcommand_from f files' -s f -l filter -d "Filter output by file type" \
    -xa "(__fish_complete_list , __fish_equery_files_filter_args)"

# has + hasuse
complete -c equery -n '__fish_seen_subcommand_from a has h hasuse' -s I -l exclude-installed -d "Exclude installed pkgs from search path"
complete -c equery -n '__fish_seen_subcommand_from a has h hasuse' -s o -l overlay-tree -d "Include overlays in search path"
complete -c equery -n '__fish_seen_subcommand_from a has h hasuse' -s p -l portage-tree -d "Include entire portage tree in search path"
#complete -c equery -n '__fish_seen_subcommand_from a has h hasuse' -s F -l format=TMPL       -d "Specify a custom output format"

# keywords
# TODO

# list
complete -c equery -n '__fish_seen_subcommand_from l list' -s d -l duplicates -d "List only installed duplicate pkgs"
complete -c equery -n '__fish_seen_subcommand_from l list' -s b -l binpkgs-missing -d "List only installed pkgs without a corresponding binary pkg"
complete -c equery -n '__fish_seen_subcommand_from l list' -s f -l full-regex -d "Query is a regex"
complete -c equery -n '__fish_seen_subcommand_from l list' -s m -l mask-reason -d "Include reason for pkg mask"
complete -c equery -n '__fish_seen_subcommand_from l list' -s I -l exclude-installed -d "Exclude installed pkgs from output"
complete -c equery -n '__fish_seen_subcommand_from l list' -s o -l overlay-tree -d "List pkgs in overlays"
complete -c equery -n '__fish_seen_subcommand_from l list' -s p -l portage-tree -d "List pkgs in the main portage tree"
#complete -c equery -n '__fish_seen_subcommand_from l list' -s F -l format=TMPL       -d "Specify a custom output format"
complete -c equery -n '__fish_seen_subcommand_from l list; and not __fish_contains_opt -s p portage-tree' \
    -xa "(__fish_print_portage_installed_pkgs)"
complete -c equery -n '__fish_seen_subcommand_from l list; and     __fish_contains_opt -s p portage-tree' \
    -xa "(__fish_print_portage_available_pkgs)"

# meta
complete -c equery -n '__fish_seen_subcommand_from m meta' -s d -l description -d "Show extended pkg description"
complete -c equery -n '__fish_seen_subcommand_from m meta' -s H -l herd -d "Show pkg's herd(s)"
complete -c equery -n '__fish_seen_subcommand_from m meta' -s k -l keywords -d "Show keywords for all matching pkg versions"
complete -c equery -n '__fish_seen_subcommand_from m meta' -s l -l license -d "Show licenses for the best matching version"
complete -c equery -n '__fish_seen_subcommand_from m meta' -s m -l maintainer -d "Show the maintainer(s) for the pkg"
complete -c equery -n '__fish_seen_subcommand_from m meta' -s S -l stablreq -d "Show STABLEREQ arches (cc's) for all matching pkg versions"
complete -c equery -n '__fish_seen_subcommand_from m meta' -s u -l useflags -d "Show per-pkg USE flag descriptions"
complete -c equery -n '__fish_seen_subcommand_from m meta' -s U -l upstream -d "Show pkg's upstream information"
complete -c equery -n '__fish_seen_subcommand_from m meta' -s x -l xml -d "Show the plain metadata.xml file"

# size
complete -c equery -n '__fish_seen_subcommand_from s size' -s b -l bytes -d "Report size in bytes"
complete -c equery -n '__fish_seen_subcommand_from s size' -s f -l full-regex -d "Query is a regex"

# uses
complete -c equery -n '__fish_seen_subcommand_from u uses' -s a -l all -d "Include all pkg versions"
complete -c equery -n '__fish_seen_subcommand_from u uses' -s i -l ignore-l10n -d "Don't show l10n USE flags"

# which
complete -c equery -n '__fish_seen_subcommand_from w which' -s m -l include-masked -d "Return highest version ebuild available"
complete -c equery -n '__fish_seen_subcommand_from w which' -s e -l ebuild -d "Print the ebuild"
