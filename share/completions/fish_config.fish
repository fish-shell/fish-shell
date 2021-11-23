complete fish_config -f
set -l prompt_commands choose save show list
set -l theme_commands choose demo dump save show list
complete fish_config -n __fish_use_subcommand -a prompt -d 'View and pick from the sample prompts'
complete fish_config -n "__fish_seen_subcommand_from prompt; and not __fish_seen_subcommand_from $prompt_commands" \
    -a choose -d 'View and pick from the sample prompts'
complete fish_config -n "__fish_seen_subcommand_from prompt; and not __fish_seen_subcommand_from $prompt_commands" \
    -a show -d 'Show what prompts would look like'
complete fish_config -n "__fish_seen_subcommand_from prompt; and not __fish_seen_subcommand_from $prompt_commands" \
    -a list -d 'List all available prompts'
complete fish_config -n "__fish_seen_subcommand_from prompt; and not __fish_seen_subcommand_from $prompt_commands" \
    -a save -d 'Save the current or given prompt to ~/.config/fish'
complete fish_config -n '__fish_seen_subcommand_from prompt; and __fish_seen_subcommand_from choose save show' -a '(fish_config prompt list)'

complete fish_config -n __fish_use_subcommand -a browse -d 'Open the web-based UI'

complete fish_config -n __fish_use_subcommand -a theme -d 'View and pick from the sample themes'
complete fish_config -n '__fish_seen_subcommand_from theme; and __fish_seen_subcommand_from choose save show' -a '(fish_config theme list)'
complete fish_config -n "__fish_seen_subcommand_from theme; and not __fish_seen_subcommand_from $theme_commands" \
    -a choose -d 'View and pick from the sample themes'
complete fish_config -n "__fish_seen_subcommand_from theme; and not __fish_seen_subcommand_from $theme_commands" \
    -a show -d 'Show what theme would look like'
complete fish_config -n "__fish_seen_subcommand_from theme; and not __fish_seen_subcommand_from $theme_commands" \
    -a list -d 'List all available themes'
complete fish_config -n "__fish_seen_subcommand_from theme; and not __fish_seen_subcommand_from $theme_commands" \
    -a save -d 'Save the given theme as universal variables'
complete fish_config -n "__fish_seen_subcommand_from theme; and not __fish_seen_subcommand_from $theme_commands" \
    -a demo -d 'Show example in the current theme'
complete fish_config -n "__fish_seen_subcommand_from theme; and not __fish_seen_subcommand_from $theme_commands" \
    -a dump -d 'Print the current theme in .theme format'
