#function __fish_emerge_print_all_pkgs_with_version_compare -d 'Print completions for all packages including the version compare if that is already typed'
#    set -l version_comparator (commandline -t | string match -r '^[\'"]*[<>]\?=\?' | \
#                               sed -r 's/^[\'"]*(.*)/\1/g')
#    set -l sedstring
#
#    if set -q $version_comparator
#        set sedstring 's/^(.*)/\1\tPackage/g'
#    else
#        set sedstring 's/^(.*)/'$version_comparator'\1\tPackage/g'
#    end
#
#    __fish_emerge_print_all_pkgs | sed -r $sedstring
#end

function __fish_emerge_print_sets
    for s in '@'(emerge --list-sets)
        switch $s
            case @profile
                printf '%s\t%s\n' $s 'set of packages deemed necessary for your system to run properly'
            case @selected-packages
                printf '%s\t%s\n' $s 'packages listed in /var/lib/portage/world'
            case @selected-sets
                printf '%s\t%s\n' $s 'sets listed in /var/lib/portage/world_sets'
            case @selected
                printf '%s\t%s\n' $s 'encompasses both the selected-packages and selected-sets sets'
            case @system
                printf '%s\t%s\n' $s 'set of packages deemed necessary for your system to run properly'
            case @world
                printf '%s\t%s\n' $s 'encompasses the selected, system and profile sets'
            case '*'
                echo $s
        end
    end
end

# TODO <ebuild|tbz2file|file|@set|atom>...
function __fish_emerge_possible_args
    if __fish_contains_opt check-news -s h help list-sets metadata regen -s r resume \
            -s s search -s S searchdesc sync -s V version
        return
        # TODO deselect=y
    else if __fish_contains_opt config -s c depclean info -s P prune -s C unmerge
        __fish_emerge_print_sets
        __fish_print_portage_installed_pkgs
        # TODO deselect=n
    else
        __fish_emerge_print_sets
        __fish_print_portage_available_pkgs
    end
end

complete -c emerge -xa "(__fish_emerge_possible_args)"

#########################
# Actions               #
#########################
complete -c emerge -l check-news
complete -c emerge -l clean
complete -c emerge -l config
complete -c emerge -s c -l depclean
complete -c emerge -l deselect
complete -c emerge -s h -l help
complete -c emerge -l info
complete -c emerge -l list-sets
complete -c emerge -l metadata
complete -c emerge -s P -l prune
complete -c emerge -l regen
complete -c emerge -s r -l resume
complete -c emerge -s s -l search
complete -c emerge -s S -l searchdesc
complete -c emerge -l sync
complete -c emerge -s C -l unmerge
complete -c emerge -s V -l version

#########################
# Options               #
#########################
complete -c emerge -s A -l alert -d "Add a terminal bell character ('\a') to all interactive prompts"
#complete -c emerge      -l alphabetical -d "Sort flag lists alphabetically"
complete -c emerge -s a -l ask -d "Prompt the user before peforming the merge"
# ask-enter-invalid
# autounmask
# autounmask-backtrack <yn>
# autounmask-continue
# autounmask-only
# autounmask-unrestricted-atoms
# autounmask-keep-keywords
# autounmask-keep-masks
# autounmask-write
complete -c emerge -l backtrack
complete -c emerge -s b -l buildpkg -d "Build a binary pkg additionally"
# buildpkg-exclude
complete -c emerge -s B -l buildpkgonly -d "Only build a binary pkg"
# changed-deps
complete -c emerge -s U -l changed-use
complete -c emerge -s l -l changelog -d "Show changelog of pkg. Use with --pretend"
complete -c emerge -l color -d "Colorized output" \
    -xa "y n"
complete -c emerge -l columns -d "Align output. Use with --pretend"
complete -c emerge -l complete-graph
# complete-graph-if-new-use <yn>
# complete-graph-if-new-ver <yn>
# config-root DIR
complete -c emerge -s d -l debug -d "Run in debug mode"
complete -c emerge -s D -l deep -d "Consider the whole dependency tree"
# depclean-lib-check
# digest
# dynamic-deps <yn>
complete -c emerge -s e -l emptytree -d "Reinstall all world pkgs"
# exclude ATOMS
complete -c emerge -l exclude
# fail-clean
complete -c emerge -s f -l fetchonly -d "Only download the pkgs but don't install them"
complete -c emerge -s F -l fetch-all-uri -d "Same as --fetchonly and grab all potential files"
# fuzzy-search
complete -c emerge -s g -l getbinpkg -d "Download infos from each binary pkg. Implies -k"
complete -c emerge -s G -l getbinpkgonly -d "As -g but don't use local infos"
complete -c emerge -l ignore-default-opts -d "Ignore EMERGE_DEFAULT_OPTS"
# ignore-build-slot-operator-deps <yn>
# ignore-soname-deps <yn>
complete -c emerge -l jobs
complete -c emerge -l keep-going
# load-average
# misspell-suggestion
# newrepo
complete -c emerge -s N -l newuse -d "Include installed pkgs with changed USE flags"
complete -c emerge -l noconfmem -d "Disregard merge records"
complete -c emerge -s O -l nodeps -d "Don't merge dependencies"
complete -c emerge -s n -l noreplace -d "Skip already installed pkgs"
complete -c emerge -l nospinner -d "Disable the spinner"
# usepkg-exclude ATOMS
# rebuild-exclude ATOMS
# rebuild-ignore ATOMS
complete -c emerge -s 1 -l oneshot -d "Don't add pkgs to world"
complete -c emerge -s o -l onlydeps -d "Only merge dependencies"
# onlydeps-with-rdeps <yn>
# package-moves
# pkg-format
# prefix DIR
complete -c emerge -s p -l pretend -d "Display what would be done without doing it"
complete -c emerge -s q -l quiet -d "Use a condensed output"
# quiet-build
# quiet-fail
# quiet-repo-display
# quiet-unmerge-warn
# rage-clean
# read-news
# rebuild-if-new-slot
# rebuild-if-new-rev
# rebuild-if-new-ver
# rebuild-if-unbuilt
# rebuild-binaries
# rebuild-binaries-timestamp TIMESTAMP
# reinstall changed-use
# reinstall-atoms ATOMS
# root-deps
# search-index
# search-similarity PERCENTAGE
complete -c emerge -s w -l select
# selective
complete -c emerge -l skipfirst -d "Remove first pkg in resume list. Use with --resume"
# sync-module <glsa|news|profiles>
complete -c emerge -s t -l tree -d "Show the dependency tree"
# unordered-display
complete -c emerge -s u -l update
# use-ebuild-visibility
# useoldpkg-atoms ATOMS
complete -c emerge -s k -l usepkg -d "Use binary pkg if available"
complete -c emerge -s K -l usepkgonly -d "Only use binary pkgs"
complete -c emerge -s v -l verbose -d "Run in verbose mode"
complete -c emerge -l verbose-conflicts -d "Verbose slot conflicts"
# verbose-slot-rebuilds
complete -c emerge -l with-bdeps -d "Pull in build time dependencies" \
    -xa "y n"
# with-bdeps-auto <yn>
# with-test-bdeps
