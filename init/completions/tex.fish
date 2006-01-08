for i in {,e}tex {,e}latex pdf{,e}latex pdf{,e}tex omega
	complete -c $i -o help -d (_ "Display help and exit")
	complete -c $i -o version -d (_ "Display version and exit")
	complete -c $i -x -a "(
		__fish_complete_suffix (commandline -ct) .tex '(La)TeX file'
	)"
end
