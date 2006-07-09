function __fish_complete_cd -d "Completions for the cd command"

	#
	# We can't simply use __fish_complete_directory because of the CDPATH
	#

	set -l wd $PWD
	set -l owd $OLDPWD

	# Check if CDPATH is set

	set -l mycdpath

	if test -z $CDPATH[1]
		set mycdpath .
	else
		set mycdpath $CDPATH
	end

	
	if echo (commandline -ct)|grep '^/\|^\./\|^\.\./' >/dev/null
		# This is an absolute search path
		eval printf '\%s\\tDirectory\\n' (commandline -ct)\*/
	else
		# This is a relative search path
		# Iterate over every directory in CDPATH 
		# and check for possible completions

		for i in $mycdpath
			# Move to the initial directory first, 
			# in case the CDPATH directory is relative

			builtin cd $wd
			builtin cd $i

			eval printf '"%s\tDirectory in "'$i'"\n"' (commandline -ct)\*/
		end
	end

	builtin cd $wd
	set OLDPWD $owd
end

