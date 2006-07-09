#
# Completions for the help command
#

for i in (builtin -n)
	complete -c help -x -a $i -d (N_ "Help for the specified builtin")
end

for i in case (sed -n < $__fish_help_dir/commands.html -e "s/.*<h2><a class=\"anchor\" name=\"\([^\"]*\)\">.*/\1/p")
	complete -c help -x -a $i -d (N_ "Help for the specified command")
end

for i in syntax todo bugs history;
	complete -c help -x -a $i -d (N_ "Help section" )
end

complete -c help -x -a completion -d (N_ "Help on how tab-completion works")
complete -c help -x -a job-control -d (N_ "Help on how job control works")
complete -c help -x -a difference -d (N_ "Summary on how fish differs from other shells")

complete -c help -x -a prompt -d (N_ "Help on how to set the prompt")
complete -c help -x -a title -d (N_ "Help on how to set the titlebar message")
complete -c help -x -a killring -d (N_ "Help on how to copy and paste")
complete -c help -x -a editor -d (N_ "Help on editor shortcuts")
complete -c help -x -a variables -d (N_ "Help on environment variables")
complete -c help -x -a color -d (N_ "Help on setting syntax highlighting colors")
complete -c help -x -a builtin-overview -d (N_ "A short summary of all builtin commands")

complete -c help -x -a globbing -d (N_ "Help on parameter expansion (Globbing)")
complete -c help -x -a expand -d (N_ "Help on parameter expansion (Globbing)")
complete -c help -x -a expand-variable -d (N_ "Help on variable expansion \$VARNAME")
complete -c help -x -a expand-home -d (N_ "Help on home directory expansion ~USER")
complete -c help -x -a expand-brace -d (N_ "Help on brace expansion {a,b,c}")
complete -c help -x -a expand-wildcard -d (N_ "Help on wildcard expansion *.*")
complete -c help -x -a expand-command-substitution -d (N_ "Help on command substitution (SUBCOMMAND)")
complete -c help -x -a expand-process -d (N_ "Help on process expansion %JOB")
