complete ghoti_config -f
set -l prompt_commands choose save show list
set -l theme_commands choose demo dump save show list
complete ghoti_config -n __ghoti_use_subcommand -a prompt -d 'View and pick from the sample prompts'
complete ghoti_config -n "__ghoti_seen_subcommand_from prompt; and not __ghoti_seen_subcommand_from $prompt_commands" \
    -a choose -d 'View and pick from the sample prompts'
complete ghoti_config -n "__ghoti_seen_subcommand_from prompt; and not __ghoti_seen_subcommand_from $prompt_commands" \
    -a show -d 'Show what prompts would look like'
complete ghoti_config -n "__ghoti_seen_subcommand_from prompt; and not __ghoti_seen_subcommand_from $prompt_commands" \
    -a list -d 'List all available prompts'
complete ghoti_config -n "__ghoti_seen_subcommand_from prompt; and not __ghoti_seen_subcommand_from $prompt_commands" \
    -a save -d 'Save the current or given prompt to ~/.config/ghoti'
complete ghoti_config -n '__ghoti_seen_subcommand_from prompt; and __ghoti_seen_subcommand_from choose save show' -a '(ghoti_config prompt list)'

complete ghoti_config -n __ghoti_use_subcommand -a browse -d 'Open the web-based UI'

complete ghoti_config -n __ghoti_use_subcommand -a theme -d 'View and pick from the sample themes'
complete ghoti_config -n '__ghoti_seen_subcommand_from theme; and __ghoti_seen_subcommand_from choose save show' -a '(ghoti_config theme list)'
complete ghoti_config -n "__ghoti_seen_subcommand_from theme; and not __ghoti_seen_subcommand_from $theme_commands" \
    -a choose -d 'View and pick from the sample themes'
complete ghoti_config -n "__ghoti_seen_subcommand_from theme; and not __ghoti_seen_subcommand_from $theme_commands" \
    -a show -d 'Show what theme would look like'
complete ghoti_config -n "__ghoti_seen_subcommand_from theme; and not __ghoti_seen_subcommand_from $theme_commands" \
    -a list -d 'List all available themes'
complete ghoti_config -n "__ghoti_seen_subcommand_from theme; and not __ghoti_seen_subcommand_from $theme_commands" \
    -a save -d 'Save the given theme as universal variables'
complete ghoti_config -n "__ghoti_seen_subcommand_from theme; and not __ghoti_seen_subcommand_from $theme_commands" \
    -a demo -d 'Show example in the current theme'
complete ghoti_config -n "__ghoti_seen_subcommand_from theme; and not __ghoti_seen_subcommand_from $theme_commands" \
    -a dump -d 'Print the current theme in .theme format'
