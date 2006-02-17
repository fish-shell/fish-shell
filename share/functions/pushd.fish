

function pushd -d (_ "Push directory to stack")
	# Comment to avoid set completions
	set -g dirstack (command pwd) $dirstack
	cd $argv[1]
end

