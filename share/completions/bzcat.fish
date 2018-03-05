
# magic completion safety check (do not remove this comment)
if not type -q bzcat
    exit
end
complete -c bzcat -x -a "(
	__fish_complete_suffix .tbz
	__fish_complete_suffix .tbz2
)
"

complete -c bzcat -x -a "(
	__fish_complete_suffix .bz
	__fish_complete_suffix .bz2
)
"

complete -c bzcat -s s -l small -d "Reduce memory usage"
