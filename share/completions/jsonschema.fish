complete -c jsonschema -s h -l help -d 'Show help'
complete -c jsonschema -l version -d 'Show version'

set subcommands validate metaschema test fmt lint bundle frame compile identify
set http_option -s h -l http -d 'Enable HTTP resolution'
set verbose_option -s v -l verbose -d 'Show verbose output'
set resolve_option -s r -l resolve -r -d 'Import the local schema'
set extension_option -s e -l extension -r -d 'Import the directory of .schema.json schemas'
set ignore_option -s i -l ignore -r -d 'Ignore the specified directory'

complete -c jsonschema -a "$subcommands" -f -n 'not __fish_seen_subcommand_from $subcommands'

set validate_condition '__fish_seen_subcommand_from validate'
complete -c jsonschema $http_option -n $validate_condition
complete -c jsonschema $verbose_option -n $validate_condition
complete -c jsonschema $resolve_option -n $validate_condition
complete -c jsonschema -l benchmark -n $validate_condition -d Benchmark

set metaschema_condition '__fish_seen_subcommand_from metaschema'
complete -c jsonschema $verbose_option -n $metaschema_condition
complete -c jsonschema $http_option -n $metaschema_condition
complete -c jsonschema $extension_option -n $metaschema_condition
complete -c jsonschema $ignore_option -n $metaschema_condition

set test_condition '__fish_seen_subcommand_from test'
complete -c jsonschema $http_option -n $test_condition
complete -c jsonschema $verbose_option -n $test_condition
complete -c jsonschema $resolve_option -n $test_condition
complete -c jsonschema $extension_option -n $test_condition
complete -c jsonschema $ignore_option -n $test_condition

set fmt_condition '__fish_seen_subcommand_from fmt'
complete -c jsonschema -s c -l check -n $fmt_condition -r -d 'Check whether schema is formatted correctly'
complete -c jsonschema -s v -l verbose -n $fmt_condition -r -d 'Show verbose output'
complete -c jsonschema $extension_option -n $fmt_condition
complete -c jsonschema $ignore_option -n $fmt_condition

set lint_condition '__fish_seen_subcommand_from lint'
complete -c jsonschema -s f -l fix -n $lint_condition -r -d 'Fix errors automatically'
complete -c jsonschema $verbose_option -n $lint_condition
complete -c jsonschema $extension_option -n $lint_condition
complete -c jsonschema $ignore_option -n $lint_condition

set bundle_condition '__fish_seen_subcommand_from bundle'
complete -c jsonschema $http_option -n $bundle_condition
complete -c jsonschema $verbose_option -n $bundle_condition
complete -c jsonschema $resolve_option -n $bundle_condition
complete -c jsonschema $extension_option -n $bundle_condition
complete -c jsonschema $ignore_option -n $bundle_condition
complete -c jsonschema -s w -l without-id -n $lint_condition -r -d "Bundle without \$id's"

set frame_condition '__fish_seen_subcommand_from frame'
complete -c jsonschema -s j -l json -n $frame_condition -d "Format output as JSON"
complete -c jsonschema $verbose_option -n $frame_condition

set identify_condition '__fish_seen_subcommand_from identify'
complete -c jsonschema -s t -l relative-to -n $identify_condition -d "Resolve the URI relative to a base"
complete -c jsonschema -s j -l json -n $identify_condition -d "Format output as JSON"
