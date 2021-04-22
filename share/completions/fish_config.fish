complete fish_config -f
set -l prompt_commands choose save show list
complete fish_config -n '__fish_use_subcommand' -a prompt -d 'View and pick from the sample prompts'
complete fish_config -n "__fish_seen_subcommand_from prompt; and not __fish_seen_subcommand_from $prompt_commands" \
    -a choose -d 'View and pick from the sample prompts'
complete fish_config -n "__fish_seen_subcommand_from prompt; and not __fish_seen_subcommand_from $prompt_commands" \
    -a show -d 'Show what prompts would look like'
complete fish_config -n "__fish_seen_subcommand_from prompt; and not __fish_seen_subcommand_from $prompt_commands" \
    -a list -d 'List all available prompts'
complete fish_config -n "__fish_seen_subcommand_from prompt; and not __fish_seen_subcommand_from $prompt_commands" \
    -a save -d 'Save the current or given prompt to ~/.config/fish'
complete fish_config -n '__fish_seen_subcommand_from prompt; and __fish_seen_subcommand_from choose save show' -a '(prompt list)'

complete fish_config -n '__fish_use_subcommand' -a browse -d 'Open the web-based UI'
