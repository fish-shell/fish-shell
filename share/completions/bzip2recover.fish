
# magic completion safety check (do not remove this comment)
if not type -q bzip2recover
    exit
end
complete -c bzip2recover -x -a "(
	__fish_complete_suffix .tbz
	__fish_complete_suffix .tbz2
)
"

complete -c bzip2recover -x -a "(
	__fish_complete_suffix .bz
	__fish_complete_suffix .bz2
)
"
