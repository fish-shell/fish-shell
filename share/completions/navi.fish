complete navi --no-files

set --local sub_commands fn help info repo widget
set --local options best-match cheatsh finder fzf-overrides fzf-overrides-var help path print query tag-rules tldr version

# subcommands
complete navi -n "not __fish_seen_subcommand_from $sub_commands && \
    not __fish_contains_opt -s h -s p -s q -s V $options" -a "$sub_commands"

set --local internal_functions "url::open welcome widget::last_command map::expand"
complete navi -n "__fish_seen_subcommand_from fn && not __fish_seen_subcommand_from $internal_functions" \
    -k -a $internal_functions

set --local supported_shells "bash zsh fish elvish"
complete navi -n "__fish_seen_subcommand_from widget && not __fish_seen_subcommand_from $supported_shells" -k -a $supported_shells

# options
complete navi -n "not __fish_seen_subcommand_from $sub_commands" -l best-match -d "Returns the best match"
complete navi -n "not __fish_seen_subcommand_from $sub_commands" -x -l cheatsh -d "Searches for cheatsheets using the cheat.sh repository"
complete navi -n "not __fish_seen_subcommand_from $sub_commands" -x -l finder -a "fzf skim" -d "Finder application to use"
complete navi -n "not __fish_seen_subcommand_from $sub_commands" -x -l fzf-overrides -d "Finder overrides for snippet selection"
complete navi -n "not __fish_seen_subcommand_from $sub_commands" -x -l fzf-overrides-var -d "Finder overrides for variable selection"
complete navi -n "not __fish_seen_subcommand_from $sub_commands" -x -s h -l help -d "Print help information"
complete navi -n "not __fish_seen_subcommand_from $sub_commands" -r -s p -l path -d "Colon-separated list of paths containing .cheat files"
complete navi -n "not __fish_seen_subcommand_from $sub_commands" -l print -d "Instead of executing a snippet, prints it to stdout"
complete navi -n "not __fish_seen_subcommand_from $sub_commands" -x -s q -l query -d "Prepopulates the search field"
complete navi -n "not __fish_seen_subcommand_from $sub_commands" -x -l tag-rules -d "[Experimental] Comma-separated list as filter for tags. ! represents negation"
complete navi -n "not __fish_seen_subcommand_from $sub_commands" -x -l tldr -d "Searches for cheatsheets using the tldr-pages repository"
complete navi -n "not __fish_seen_subcommand_from $sub_commands" -x -s V -l version -d "Print version information"
