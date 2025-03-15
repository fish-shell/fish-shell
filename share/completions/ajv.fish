set -l command ajv

complete -c $command -f

set -l subcommands_with_descriptions 'validate\t"Validate the files against a schema"' \
    'compile\t"Compile the schema"' \
    'migrate\t"Migrate the schemas"' \
    'test\t"Check whether the files are valid against a schema"' \
    'help\t"Show help"'

set -l subcommands (string replace --regex '\\\t.+' '' -- $subcommands_with_descriptions)

complete -c $command \
    -a "$subcommands_with_descriptions" \
    -n "not __fish_seen_subcommand_from $subcommands"

set -l subcommands_without_help $subcommands[1..-2]

complete -c $command -s s -F -r \
    -d 'The schema used for a validation' \
    -n "__fish_seen_subcommand_from $subcommands_without_help"

complete -c $command -s d -F -r \
    -d 'The files validated against a schema' \
    -n "__fish_seen_subcommand_from validate test"

complete -c $command -s o -F -r \
    -d 'The output file of a migrated schema' \
    -n "__fish_seen_subcommand_from validate migrate"

for flag in valid invalid
    complete -c $command -l $flag \
        -d "Check whether the files are $flag against a schema" \
        -n "__fish_seen_subcommand_from test"
end

complete -c $command \
    -a 'draft7\tdefault draft2019 draft2020 jtd' \
    -l spec -x \
    -d 'Specify the draft used for a schema' \
    -n "__fish_seen_subcommand_from $subcommands_without_help"
