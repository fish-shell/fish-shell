function __fish_winetricks__complete_verbs
    winetricks list-all |
        string match --invert --regex '^==' |
        string match --invert --regex '^(apps|dlls|fonts|games|settings)$' |
        string replace --regex '(\S+)\s+(.+)' '$1\t$2'
end

set -l command winetricks

complete -c $command -f

complete -c $command -s h -l help -d 'Show help'
complete -c $command -s V -l version -d 'Show version'

complete -c $command -l country -f -d 'Specify the country code'

complete -c $command -s f -l force \
    -d "Don't check whether packages were already installed"

complete -c $command -l gui -d 'Show gui diagnostics'
complete -c $command -l isolate -d 'Use the separate bottles for applications'
complete -c $command -l self-update -d 'Update to the latest version'
complete -c $command -l update-rollback -d 'Rollback the latest self-update'
complete -c $command -s k -l keep_isos -d 'Cache ISOs'
complete -c $command -l no-clean -d "Don't delete temporary directories"
complete -c $command -s q -l unattended -d "Don't show any prompts"

complete -c $command -s r -l ddrescue \
    -d 'Reattempt when caching scratched discs'

complete -c $command -s t -l torify -d 'Run downloads through torify'
complete -c $command -l virefy -d 'Test verbs automaitally'
complete -c $command -s v -l verbose -d 'Show commands'

set -l subcommands_with_descriptions 'list\t"List categories"' \
    'list-all\t"List categories and their verbs"' \
    'apps\t"Interact with applications"' \
    'benchmarks\t"Interact with benchmarks"' \
    'dlls\t"Interact with DLLs"' \
    'fonts\t"Interact with fonts"' \
    'games\t"Interact with games"' \
    'settings\t"Interact with settings"' \
    'list-cached\t"Interact with list-cached"' \
    'list-download\t"Interact with list-download"' \
    'list-manual-download\t"Interact with list-manual-download"' \
    'list-installed\t"Interact with list-installed"' \
    'prefix\t"Specify the bottle"' \
    'annihilate\t"Clear the bottle"'

for architecture in 32 64
    set -a subcommands_with_descriptions "arch=$architecture\t\"Specify the architecture\""
end

set -l subcommands (string replace --regex '\\\t.+' '' -- $subcommands_with_descriptions)
set -l root_conditon "not __fish_seen_subcommand_from $subcommands"

complete -c $command -a "$subcommands_with_descriptions" -n $root_conditon

complete -c $command -a list \
    -d 'List applications' \
    -n '__fish_seen_subcommand_from apps'

complete -c $command -a list \
    -d 'List DLLs' \
    -n '__fish_seen_subcommand_from dlls'

for subcommand in benchmarks fonts games settings
    complete -c $command -a list \
        -d 'List $subcommand' \
        -n '__fish_seen_subcommand_from dlls'
end

complete -c $command -a '(__fish_winetricks__complete_verbs)' \
    -d 'Specify the verb or path to it' \
    -n $root_conditon
