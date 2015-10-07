function __fish_complete_cd -d "Completions for the cd command"
	set -l cdpath $CDPATH
	[ -z "$cdpath" ]; and set cdpath "."
	# Remove the real path to "." (i.e. $PWD) from cdpath if we're in it
	# so it doesn't get printed in the descriptions
	set -l ind
	if begin; set ind (contains -i -- $PWD $cdpath)
		      and contains -- "." $cdpath
	          end
			  set -e cdpath[$ind]
    end
	for i in $cdpath
		set -l desc
		# Don't show description for current directory
		# and replace $HOME with "~"
		[ $i = "." ]; or set -l desc (string replace -r -- "^$HOME" "~" "$i")
		pushd $i
		for d in (commandline -ct)*/
			# Check if it's accessible - the glob only matches directories
			[ -x $d ]; and printf "%s\t%s\n" $d $desc
		end
		popd
	end
end
