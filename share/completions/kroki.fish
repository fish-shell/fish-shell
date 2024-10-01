set -l command kroki

complete -c $command -f

complete -c $command -s h -l help -d 'Show help'
complete -c $command -s v -l version -d 'Show version'

set -l subcommands_with_descriptions 'help\t"Show help"' \
    'version\t"Show version"' \
    'convert\t"Convert files"'

set -l subcommands (string replace --regex '\\\t.+' '' -- $subcommands_with_descriptions)

complete -c $command -a "$subcommands_with_descriptions" \
    -n "not __fish_seen_subcommand_from $subcommands"

set -l convert_condition '__fish_seen_subcommand_from convert'

complete -c $command -s c -l config -r \
    -d 'Specify the configuration file for a diagram' \
    -n $convert_condition

complete -c $command -s f -l format -x \
    -a 'svg jpg png pdf' \
    -d 'Specify the output file format for a diagram' \
    -n $convert_condition

complete -c $command -s o -l out-file -r \
    -d 'Specify the output file for a diagram' \
    -n $convert_condition

complete -c $command -s t -l type -x \
    -a 'actdiag blockdiag c4plantuml ditaa dot erd graphviz nomnoml nwdiag
plantuml seqdiag svgbob symbolator umlet vega vegalite' \
    -d 'Specify the type of a diagram' \
    -n $convert_condition
