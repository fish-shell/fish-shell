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
		[ $i = "." ]; or set -l desc (echo $i | sed -e "s|$HOME|~|")
		# Save the real path so we can go back after cd-ing in
		# even if $i is "."
		set -l real $PWD
		pushd $i
		for d in (commandline -ct)*
			if begin [ -d $d ]
				builtin cd $d ^/dev/null # to check permission
				and builtin cd $real ^/dev/null # go back
			   end
			   printf "%s/\t%s\n" $d $desc
			end
		end
		popd
	end
end
