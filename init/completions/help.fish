#
# Completions for the help command
#

for i in (builtin -n)
	complete -c help -x -a $i -d 'Help for the '$i' builtin'
end

for i in count dirh dirs help mimedb nextd open popd prevd pushd set_color tokenize psub umask type 
	complete -c help -x -a $i -d 'Help for the '$i' command'
end

for i in syntax todo bugs history;
	complete -c help -x -a $i -d 'Help section on '$i
end

complete -c help -x -a completion -d "Help on how tab-completion works"
complete -c help -x -a job-control -d "Help on how job control works"
complete -c help -x -a difference -d "Summary on how fish differs from other shells"

complete -c help -x -a prompt -d "Help on how to set the prompt"
complete -c help -x -a title -d "Help on how to set the titlebar message"
complete -c help -x -a killring -d "Help on how to copy and paste"
complete -c help -x -a editor -d "Help on editor shortcuts"
complete -c help -x -a expand -d "Help on parameter expantion (Globbing)"
complete -c help -x -a globbing -d "Help on parameter expantion (Globbing)"
complete -c help -x -a variables -d "Help on environment variables"
complete -c help -x -a color -d "Help on setting syntax highlighting colors"
complete -c help -x -a prompt -d "Help on changing the prompt"
complete -c help -x -a title -d "Help on changing the titlebar messages"
complete -c help -x -a builtin-overview -d "A short summary of all builtin commands"
complete -c help -x -a changes -d "The changelog"

