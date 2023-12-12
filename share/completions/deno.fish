deno completions fish | source

# complete deno task
complete -f -c deno -n "__fish_seen_subcommand_from task" -n "__fish_is_nth_token 2" -a "(NO_COLOR=1 deno task &| string match -rg '^- (\S*)')"
