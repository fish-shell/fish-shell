
complete -c bru -s h -l help -d "Show help"
complete -c bru -l version -d "Show version number"

complete -c bru -a run -d "Run a request"

complete -c bru -s r --condition '__fish_seen_subcommand_from run' -d "Indicates a recursive run"
complete -c bru -l cacert --condition '__fish_seen_subcommand_from run' -d "CA certificate to verify peer against"
complete -c bru -l env --condition '__fish_seen_subcommand_from run' -d "Environment variables"
complete -c bru -l env-var --condition '__fish_seen_subcommand_from run' -d "Overwrite a single environment variable, multiple usages possible"
complete -c bru -s o -l output --condition '__fish_seen_subcommand_from run' -d "Path to write file results to"
complete -c bru -s f -l format --condition '__fish_seen_subcommand_from run' -d "Format of the file results" -xa "json junit html"
complete -c bru -l insecure --condition '__fish_seen_subcommand_from run' -d "Allow insecure connections"
complete -c bru -l tests-only --condition '__fish_seen_subcommand_from run' -d "Only run requests that have a test"
complete -c bru -l bail --condition '__fish_seen_subcommand_from run' -d "Stop execution after a failure of a request, test or assertion"
