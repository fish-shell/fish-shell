function __fish_complete_cd -d "Completions for the cd command"
	set -l token (commandline -ct)
	# Absolute path or explicitly from the current directory - no descriptions and no CDPATH
	if string match -qr '^\.?\.?/.*' -- $token
		for d in $token*/
			# Check if it's accessible - the glob only matches directories
			[ -x $d ]; and printf "%s\n" $d
		end
	else # Relative path - check $CDPATH and use that as description
		set -l cdpath $CDPATH
		[ -z "$cdpath" ]; and set cdpath "."
		# Remove the real path to "." (i.e. $PWD) from cdpath if we're in it
		# so it doesn't get printed in the descriptions
		if set -l ind (contains -i -- $PWD $cdpath)
			and contains -- "." $cdpath
			set -e cdpath[$ind]
		end
		# TODO: There's a subtlety regarding descriptions - if $cdpath[1]/foo and $cdpath[2]/foo exist, we print both
		# but want the first description to win - this currently works, but is not guaranteed
		for i in $cdpath
			set -l desc
			# Don't show description for current directory
			# and replace $HOME with "~"
			[ $i = "." ]; or set -l desc (string replace -r -- "^$HOME" "~" "$i")
			# This assumes the CDPATH component itself is cd-able
			for d in $i/$token*/
				# Remove the cdpath component again
				[ -x $d ]; and printf "%s\t%s\n" (string replace -r "^$i/" "" -- $d) $desc
			end
		end
	end
end
