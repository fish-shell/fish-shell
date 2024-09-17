set command ajv

complete -c $command -f

set subcommands_with_descriptions 'validate\t"Validate the files against a schema"' \
    'compile\t"Compile the schema"' \
    'migrate\t"Migrate the schemas"' \
    'test\t"Check whether the files are valid against a schema"' \
    'help\t"Show help"'

set subcommands (string replace --regex '\\\t.+' '' -- $subcommands_with_descriptions)

complete -c $command \
    -a "$subcommands_with_descriptions" \
    -n "not __fish_seen_subcommand_from $subcommands"

set subcommands_without_help $subcommands[1..-2]

complete -c $command \
    -s s \
    -d 'The [s]chema used for a validation' \
    -n "__fish_seen_subcommand_from $subcommands_without_help" \
    -F -r

complete -c $command \
    -s d \
    -d 'The files validated against a schema' \
    -n "__fish_seen_subcommand_from validate test" \
    -F -r

complete -c $command \
    -s o \
    -d 'The output file of a migrated schema' \
    -n "__fish_seen_subcommand_from validate migrate" \
    -F -r

for flag in valid invalid
    complete -c $command \
        -l $flag \
        -d "Check whether the files are $flag against a schema" \
        -n "__fish_seen_subcommand_from test"
end

complete -c $command \
    -a 'draft7\tdefault draft2019 draft2020 jtd' \
    -l spec \
    -d 'Specify the draft used for a schema' \
    -n "__fish_seen_subcommand_from $subcommands_without_help" \
    -x
