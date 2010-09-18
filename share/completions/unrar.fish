
# Peek inside of archives and list all files
complete -c unrar -a "(__fish_complete_unrar)"

complete -x -c unrar -n '__fish_use_subcommand' -a e --description "Extract files to current directory"
complete -x -c unrar -n '__fish_use_subcommand' -a l --description "List archive"
complete -x -c unrar -n '__fish_use_subcommand' -a lt --description "List archive (technical)"
complete -x -c unrar -n '__fish_use_subcommand' -a lb --description "List archive (bare)"
complete -x -c unrar -n '__fish_use_subcommand' -a p --description "Print file to stdout"
complete -x -c unrar -n '__fish_use_subcommand' -a t --description "Test archive files"
complete -x -c unrar -n '__fish_use_subcommand' -a v --description "Verbosely list archive"
complete -x -c unrar -n '__fish_use_subcommand' -a vt --description "Verbosely list archive (technical)"
complete -x -c unrar -n '__fish_use_subcommand' -a vb --description "Verbosely list archive (bare)"
complete -x -c unrar -n '__fish_use_subcommand' -a x --description "Extract files with full path"

