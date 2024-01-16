_PIPENV_COMPLETE=fish_source __fish_cache_sourced_completions pipenv 2>/dev/null
or _PIPENV_COMPLETE=fish_source pipenv 2>/dev/null | source

# manual workaround for pipenv run command completion until this is supported by the built-in mechanism
complete -c pipenv -n "__fish_seen_subcommand_from run" -a "(__fish_complete_subcommand --fcs-skip=2)" -x
