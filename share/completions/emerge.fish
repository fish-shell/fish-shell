#function __fish_emerge_print_all_pkgs_with_version_compare -d 'Print completions for all packages including the version compare if that is already typed'
#    set -l version_comparator (commandline -t | string match -r '^[\'"]*[<>]\?=\?' |
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
complete -c emerge -l check-news -d "Display total of unread GLEP 42 news items"
complete -c emerge -l clean -d "Remove all but the most recently installed version in each slot"
complete -c emerge -l config -d "Run package-specific actions after emerge (usually config setup)"
complete -c emerge -s c -l depclean -d "Remove orphaned dependencies (packages not in @world and not required by other packages)"
complete -c emerge -s W -l deselect -a 'y n' -d "Remove atoms/sets from @world. Implied by uninstall actions (use --deselect=n to prevent)"
complete -c emerge -s h -l help -d "Display help information"
complete -c emerge -l info -d "Display ancillary information required for bug reports"
complete -c emerge -l list-sets -d "Display a list of available package sets"
complete -c emerge -l metadata
complete -c emerge -s P -l prune -d "Removes all but the highest installed version of a package from your system"
complete -c emerge -l regen -d "Check and update the dependency cache of all ebuilds in repository"
complete -c emerge -s r -l resume -d "Resumes the most recent merge list that aborted due to an error"
complete -c emerge -s s -l search -d "Search ebuilds for <string>. Use `%<string>` for regex"
complete -c emerge -s S -l searchdesc -d "Like --search, but also matches package descriptions"
complete -c emerge -l sync -d "Update repositories. Use without arguments to update all autosync repos"
complete -c emerge -s C -l unmerge -d "Remove all matching packages without checking dependencies"
complete -c emerge -s V -l version -d "Display version number"

#########################
# Options               #
#########################
complete -c emerge -l accept-properties -d "Temporarily override ACCEPT_PROPERTIES (e.g. =-interactive)"
complete -c emerge -l accept-restrict -d "Temporarily override ACCEPT_RESTRICT (e.g. =-bindist)"
complete -c emerge -s A -l alert -a 'y n' -d "Add a terminal bell character ('\a') to all interactive prompts"
complete -c emerge -l alphabetical -d "Sort flag lists alphabetically"
complete -c emerge -s a -l ask -a 'y n' -d "Prompt the user before performing the merge"
complete -c emerge -l ask-enter-invalid -d "Makes --ask reject single Enter keypresses as answers"
complete -c emerge -l autounmask -a 'y n' -d "Automatically unmasks packages and generates package.use settings"
complete -c emerge -l autounmask-backtrack -xa 'y n' -d "Allow backtracking after autounmask detects necessary changes"
complete -c emerge -l autounmask-continue -a 'y n' -d "Apply autounmask changes and continue to execute command"
complete -c emerge -l autounmask-only -a 'y n' -d "Only unmask and generate package.use settings without building anything"
complete -c emerge -l autounmask-unrestricted-atoms -a 'y n' -d "Write keyword/mask changes using >= operators instead of ="
complete -c emerge -l autounmask-keep-keywords -a 'y n' -d "Prevent autounmask from creating package.accept_keywords changes"
complete -c emerge -l autounmask-keep-masks -a 'y n' -d "Prevent autounmask from creating package.unmask or ** keyword changes"
complete -c emerge -l autounmask-license -xa 'y n' -d "Allow autounmask to create package.license changes"
complete -c emerge -l autounmask-use -xa 'y n' -d "Allow autounmask to create package.use changes (enabled by default)"
complete -c emerge -l autounmask-write -a 'y n' -d "Write autounmask changes to config files (implied by --ask)"
complete -c emerge -l backtrack -d "Max number of times to backtrack on dependency conflicts (default: 20)"
complete -c emerge -l binpkg-changed-deps -a 'y n' -d "Ignore binary packages whose ebuild dependencies changed since the build"
complete -c emerge -l binpkg-respect-use -a 'y n' -d "Ignore binary packages whose USE flags differ from current config"
complete -c emerge -s b -l buildpkg -a 'y n' -d "Build a binary package in addition to merging"
complete -c emerge -l buildpkg-exclude -d "Atoms for which no binary packages should be built"
complete -c emerge -s B -l buildpkgonly -d "Only build a binary pkg"
complete -c emerge -l changed-deps -a 'y n' -d "Replace packages whose ebuild dependencies changed since they were built"
complete -c emerge -l changed-deps-report -a 'y n' -d "Report packages whose ebuild dependencies changed since installation"
complete -c emerge -l changed-slot -a 'y n' -d "Replace packages whose ebuild SLOT metadata changed since they were built"
complete -c emerge -s U -l changed-use -d "Reinstall packages whose active USE flags changed since installation (ignores added/removed disabled flags)"
complete -c emerge -l color -d "Colorized output" \
    -xa "y n"
complete -c emerge -l columns -d "Align output. Use with --pretend"
complete -c emerge -l complete-graph -a 'y n' -d "Consider deep deps of all @world packages; bail if the operation would break any"
complete -c emerge -l complete-graph-if-new-use -xa 'y n' -d "Trigger --complete-graph when USE or IUSE changes for an installed package (default: y)"
complete -c emerge -l complete-graph-if-new-ver -xa 'y n' -d "Trigger --complete-graph when an installed package version changes (default: y)"
complete -c emerge -l config-root -d "Set PORTAGE_CONFIGROOT"
complete -c emerge -s d -l debug -d "Run in debug mode"
complete -c emerge -s D -l deep -d "Consider the whole dependency tree"
complete -c emerge -l depclean-lib-check -a 'y n' -d "Account for library link-level deps during --depclean and --prune (default: y)"
complete -c emerge -l digest -d "Regenerate package manifests (prefer repoman manifest)"
complete -c emerge -l dynamic-deps -xa 'y n' -d "Substitute installed-package deps with current ebuild deps during calculations (default: y)"
complete -c emerge -s e -l emptytree -d "Reinstall all world pkgs"
complete -c emerge -s X -l exclude -d "Space-separated atoms to never install"
complete -c emerge -l fail-clean -a 'y n' -d "Clean up temp files after a build failure (useful with PORTAGE_TMPDIR on tmpfs)"
complete -c emerge -s f -l fetchonly -d "Only download the pkgs but don't install them"
complete -c emerge -s F -l fetch-all-uri -d "Same as --fetchonly and grab all potential files"
complete -c emerge -l fuzzy-search -a 'y n' -d "Match search strings approximately; auto-disabled for regex searches (default: y)"
complete -c emerge -s g -l getbinpkg -a 'y n' -d "Download infos from each binary pkg. Implies -k"
complete -c emerge -s G -l getbinpkgonly -a 'y n' -d "As -g but don't use local infos"
complete -c emerge -l getbinpkg-exclude -d "Remote binary packages matching these atoms are not fetched"
complete -c emerge -l getbinpkg-include -d "Only fetch remote binary packages matching these atoms"
complete -c emerge -l ignore-default-opts -d "Ignore EMERGE_DEFAULT_OPTS"
complete -c emerge -l ignore-built-slot-operator-deps -xa 'y n' -d "Ignore slot/sub-slot := operator deps recorded at build time (debug only)"
complete -c emerge -l ignore-soname-deps -xa 'y n' -d "Ignore soname dependencies of binary and installed packages (default: y)"
complete -c emerge -l ignore-world -a 'y n' -d "Ignore @world deps; may break installed packages (use with --ask)"
complete -c emerge -l implicit-system-deps -xa 'y n' -d "Assume packages may have implicit deps on @system (default: y)"
complete -c emerge -s j -l jobs -d "Number of packages to build simultaneously (0 = CPU count)"
complete -c emerge -l jobs-tmpdir-require-free-gb -d "Minimum free GiB in PORTAGE_TMPDIR before starting more jobs (default: 18)"
complete -c emerge -l keep-going -a 'y n' -d "Continue as much as possible after an error; recalculate deps for remaining packages"
complete -c emerge -s l -l load-average -d "Do not start new builds if load average is at least LOAD"
complete -c emerge -l misspell-suggestions -xa 'y n' -d "Show similar package names when a package is not found (default: y)"
complete -c emerge -l newrepo -d "Recompile a package if it is now pulled from a different repository"
complete -c emerge -s N -l newuse -d "Include installed pkgs with changed USE flags"
complete -c emerge -l nobindeps -d "Use ebuilds for deps; binary packages only for explicitly named atoms"
complete -c emerge -l noconfmem -d "Disregard merge records"
complete -c emerge -s O -l nodeps -d "Don't merge dependencies"
complete -c emerge -s n -l noreplace -d "Skip already installed pkgs"
complete -c emerge -l nospinner -d "Disable the spinner"
complete -c emerge -l usepkg-exclude -d "Ignore matching binary packages"
complete -c emerge -l usepkg-exclude-live -a 'y n' -d "Do not install from binary packages for live ebuilds"
complete -c emerge -l usepkg-include -d "Ignore non-matching binary packages"
complete -c emerge -l useoldpkg-atoms -d "Prefer matching binary packages over newer unbuilt ebuilds"
complete -c emerge -l rebuild-exclude -d "Do not rebuild matching packages due to --rebuild"
complete -c emerge -l rebuild-ignore -d "Do not rebuild packages that depend on matching packages due to --rebuild"
complete -c emerge -s 1 -l oneshot -d "Don't add pkgs to world"
complete -c emerge -s o -l onlydeps -d "Only merge dependencies"
complete -c emerge -l onlydeps-with-rdeps -xa 'y n' -d "Include run-time deps when using --onlydeps (default: y)"
complete -c emerge -l onlydeps-with-ideps -xa 'y n' -d "Include install-time deps when --onlydeps and --onlydeps-with-rdeps=n are used"
complete -c emerge -l package-moves -a 'y n' -d "Apply package moves when necessary (default: y; do not disable normally)"
complete -c emerge -l pkg-format -xa 'tar rpm' -d "Binary package format to create"
complete -c emerge -l prefix -d "Set EPREFIX"
complete -c emerge -l quickpkg-direct -a 'y n' -d "Use installed packages directly as binary packages"
complete -c emerge -l quickpkg-direct-root -d "Root to use as --quickpkg-direct source (default: /)"
complete -c emerge -s p -l pretend -d "Display what would be done without doing it"
complete -c emerge -s q -l quiet -a 'y n' -d "Use a condensed output"
complete -c emerge -l quiet-build -a 'y n' -d "Redirect all build output to logs; display on failure"
complete -c emerge -l quiet-fail -a 'y n' -d "Suppress build log display on failure (show only die message and log path)"
complete -c emerge -l quiet-repo-display -d "Suppress ::repository in merge list; use numbers instead"
complete -c emerge -l quiet-unmerge-warn -d "Suppress the warning shown before --unmerge actions"
complete -c emerge -l rage-clean -d "Like --unmerge but with CLEAN_DELAY=0; can remove critical packages"
complete -c emerge -l read-news -a 'y n' -d "Offer to read unread news via eselect when --ask is active"
complete -c emerge -l rebuild-if-new-slot -a 'y n' -d "Rebuild packages when a new slot can satisfy := deps (default: y)"
complete -c emerge -l rebuild-if-new-rev -a 'y n' -d "Rebuild packages when build-time deps are installed with a new revision"
complete -c emerge -l rebuild-if-new-ver -a 'y n' -d "Rebuild packages when build-time deps are installed with a new version"
complete -c emerge -l rebuild-if-unbuilt -a 'y n' -d "Rebuild packages when build-time deps are built from source"
complete -c emerge -l rebuilt-binaries -a 'y n' -d "Replace installed packages with binary packages that have been rebuilt"
complete -c emerge -l rebuilt-binaries-timestamp -d "Only consider rebuilt binaries with BUILD_TIME newer than TIMESTAMP"
complete -c emerge -l regex-search-auto -xa 'y n' -d "Auto-detect regex in search strings (default: y)"
# reinstall changed-use (alias for --changed-use)
complete -c emerge -l reinstall-atoms -d "Treat matching packages as not installed and reinstall them"
complete -c emerge -l root -d "Set ROOT (target root filesystem)"
complete -c emerge -l root-deps -d "Control whether build-time deps go into ROOT or /"
complete -c emerge -l sysroot -d "Set SYSROOT (root for DEPEND build-time deps)"
complete -c emerge -l search-index -xa 'y n' -d "Use the egencache-built search index (default: y)"
complete -c emerge -l search-similarity -d "Minimum similarity percentage for fuzzy search results (default: 80)"
complete -c emerge -s w -l select -a 'y n' -d "Add packages to @world (inverse of --oneshot)"
complete -c emerge -l selective -a 'y n' -d "Skip packages already installed (same as --noreplace; use =n to force disable)"
complete -c emerge -l skipfirst -d "Remove first pkg in resume list. Use with --resume"
complete -c emerge -l sync-submodule -xa 'glsa news profiles' -d "Restrict --sync to specified submodule (rsync only); repeatable"
complete -c emerge -s t -l tree -d "Show the dependency tree"
complete -c emerge -l unordered-display -d "Remove merge-order constraint from --tree output for readability"
complete -c emerge -s u -l update -d "Update packages to best available version"
complete -c emerge -l update-if-installed -d "Like --update but only for packages already installed"
complete -c emerge -l use-ebuild-visibility -a 'y n' -d "Use unbuilt ebuild metadata for visibility checks on built packages"
complete -c emerge -s k -l usepkg -a 'y n' -d "Use binary pkg if available"
complete -c emerge -s K -l usepkgonly -a 'y n' -d "Only use binary pkgs"
complete -c emerge -s v -l verbose -a 'y n' -d "Run in verbose mode"
complete -c emerge -l verbose-conflicts -d "Verbose slot conflicts"
complete -c emerge -l verbose-missing-ebuilds -a 'y n' -d "Warn on @world packages with no available ebuild (default: y)"
complete -c emerge -l verbose-slot-rebuilds -a 'y n' -d "List packages causing slot rebuilds in emerge output (default: y)"
complete -c emerge -l with-bdeps -d "Pull in build time dependencies" \
    -xa "y n"
complete -c emerge -l with-bdeps-auto -xa 'y n' -d "Auto-enable --with-bdeps for installation actions (default: y)"
complete -c emerge -l with-test-deps -a 'y n' -d "Pull in test-conditional deps for matched packages even if test is not in FEATURES"
