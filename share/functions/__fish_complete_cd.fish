function __fish_complete_cd -d "Completions for the cd command"
	#
	# We can't simply use __fish_complete_directories because of the CDPATH
	#
	set -l wd $PWD

	# Check if CDPATH is set

	set -l mycdpath

	if test -z $CDPATH[1]
		set mycdpath .
	else
		set mycdpath $CDPATH
	end

	# Note how this works: we evaluate $ctoken*/
	# That trailing slash ensures that we only expand directories

	set -l ctoken (commandline -ct)
	if echo $ctoken | __fish_sgrep '^/\|^\./\|^\.\./\|^~/' >/dev/null
		# This is an absolute search path
		# Squelch descriptions per issue 254
		eval printf '\%s\\n' $ctoken\*/
	else
		# This is a relative search path
		# Iterate over every directory in CDPATH
		# and check for possible completions

		for i in $mycdpath
			# Move to the initial directory first,
			# in case the CDPATH directory is relative
			builtin cd $wd ^/dev/null
			builtin cd $i ^/dev/null
			
			if test $status -ne 0
				# directory does not exists or missing permission
				continue
			end

			# What we would really like to do is skip descriptions if all
			# valid paths are in the same directory, but we don't know how to
			# do that yet; so instead skip descriptions if CDPATH is just .
			if test "$mycdpath" = .
				eval printf '"%s\n"' $ctoken\*/
			else
				eval printf '"%s\tin "'$i'"\n"' $ctoken\*/
			end
		end
	end

	builtin cd $wd ^/dev/null
end
