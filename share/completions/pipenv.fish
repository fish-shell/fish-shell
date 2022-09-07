# try new completion generation of click first
set -l comps (_PIPENV_COMPLETE=fish_source pipenv 2>/dev/null)
if test -z "$comps" || string match -q 'Usage:*' -- $comps
    # fall back to older click-completion used in prior versions
    set comps (_PIPENV_COMPLETE=source-fish pipenv --completion 2>/dev/null)
end

set -q comps[1] && printf %s\n $comps | source


# manual workaround for pipenv run command completion until this is supported by the built-in mechanism
complete -c pipenv -n "__fish_seen_subcommand_from run" -a "(__fish_complete_subcommand --fcs-skip=2)" -x
