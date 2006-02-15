
function popd -d (_ "Pop dir from stack")
	if test $dirstack[1]
		cd $dirstack[1]
	else
		printf (_ "%s: Directory stack is empty...") popd
		return 1
	end

	set -e dirstack[1]

end

