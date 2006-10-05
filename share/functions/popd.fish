
function popd -d (N_ "Pop dir from stack")
	if test $dirstack[1]
		cd $dirstack[1]
	else
		printf (_ "%s: Directory stack is empty...\n") popd
		return 1
	end

	set -e dirstack[1]

end

