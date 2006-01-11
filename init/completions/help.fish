#
# Completions for the help command
#

for i in (builtin -n)
	complete -c help -x -a $i -d (printf (_ "Help for the '%s' builtin") $i)
end

for i in count dirh dirs help mimedb nextd open popd prevd pushd set_color psub umask type 
	complete -c help -x -a $i -d (printf (_ "Help for the '%s' command") $i )
end

for i in syntax todo bugs history;
	complete -c help -x -a $i -d (printf (_ "Help section on %s") $i)
end

complete -c help -x -a completion -d (_ "Help on how tab-completion works")
complete -c help -x -a job-control -d (_ "Help on how job control works")
complete -c help -x -a difference -d (_ "Summary on how fish differs from other shells")

complete -c help -x -a prompt -d (_ "Help on how to set the prompt")
complete -c help -x -a title -d (_ "Help on how to set the titlebar message")
complete -c help -x -a killring -d (_ "Help on how to copy and paste")
complete -c help -x -a editor -d (_ "Help on editor shortcuts")
complete -c help -x -a variables -d (_ "Help on environment variables")
complete -c help -x -a color -d (_ "Help on setting syntax highlighting colors")
complete -c help -x -a builtin-overview -d (_ "A short summary of all builtin commands")

complete -c help -x -a globbing -d (_ "Help on parameter expansion (Globbing)")
complete -c help -x -a expand -d (_ "Help on parameter expansion (Globbing)")
complete -c help -x -a expand-variable -d (_ "Help on variable expansion \$VARNAME")
complete -c help -x -a expand-home -d (_ "Help on home directory expansion ~USER")
complete -c help -x -a expand-brace -d (_ "Help on brace expansion {a,b,c}")
complete -c help -x -a expand-wildcard -d (_ "Help on wildcard expansion *.*")
complete -c help -x -a expand-command-substitution -d (_ "Help on command substututions (SUBCOMMAND)")
complete -c help -x -a expand-process -d (_ "Help on process expansion %JOB")
