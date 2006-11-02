
# Peek inside of archives and list all files
complete -c unrar -a "(__fish_complete_unrar)"

complete -x -c unrar -n '__fish_use_subcommand' -a e -d (N_  "Extract files to current directory")
complete -x -c unrar -n '__fish_use_subcommand' -a l -d (N_ "List archive" )
complete -x -c unrar -n '__fish_use_subcommand' -a lt -d (N_ "List archive (technical)" )
complete -x -c unrar -n '__fish_use_subcommand' -a lb -d (N_ "List archive (bare)" )
complete -x -c unrar -n '__fish_use_subcommand' -a p -d (N_  "Print file to stdout")
complete -x -c unrar -n '__fish_use_subcommand' -a t -d (N_  "Test archive files")
complete -x -c unrar -n '__fish_use_subcommand' -a v -d (N_  "Verbosely list archive")
complete -x -c unrar -n '__fish_use_subcommand' -a vt -d (N_  "Verbosely list archive (technical)")
complete -x -c unrar -n '__fish_use_subcommand' -a vb -d (N_  "Verbosely list archive (bare)")
complete -x -c unrar -n '__fish_use_subcommand' -a x -d (N_  "Extract files with full path")

