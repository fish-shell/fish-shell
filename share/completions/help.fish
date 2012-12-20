#
# Completions for the help command
#

if test -f "$__fish_help_dir/commands.html"
        for i in case (sed -n < $__fish_help_dir/commands.html -e "s/.*<h[12]><a class=\"anchor\" name=\"\([^\"]*\)\">.*/\1/p")
		complete -c help -x -a $i --description "Help for the specified command"
	end
end

complete -c help -x -a syntax --description 'Introduction to the fish syntax'
complete -c help -x -a todo --description 'Incomplete aspects of fish'
complete -c help -x -a bugs --description 'Known fish bugs'
complete -c help -x -a history --description 'Help on how to reuse previously entered commands'

complete -c help -x -a completion --description "Help on how tab-completion works"
complete -c help -x -a job-control --description "Help on how job control works"
complete -c help -x -a difference --description "Summary on how fish differs from other shells"

complete -c help -x -a prompt --description "Help on how to set the prompt"
complete -c help -x -a title --description "Help on how to set the titlebar message"
complete -c help -x -a killring --description "Help on how to copy and paste"
complete -c help -x -a editor --description "Help on editor shortcuts"
complete -c help -x -a variables --description "Help on environment variables"
complete -c help -x -a color --description "Help on setting syntax highlighting colors"

complete -c help -x -a globbing --description "Help on parameter expansion (Globbing)"
complete -c help -x -a expand --description "Help on parameter expansion (Globbing)"
complete -c help -x -a expand-variable --description "Help on variable expansion \$VARNAME"
complete -c help -x -a expand-home --description "Help on home directory expansion ~USER"
complete -c help -x -a expand-brace --description "Help on brace expansion {a,b,c}"
complete -c help -x -a expand-wildcard --description "Help on wildcard expansion *.*"
complete -c help -x -a expand-command-substitution --description "Help on command substitution (SUBCOMMAND)"
complete -c help -x -a expand-process --description "Help on process expansion %JOB"
