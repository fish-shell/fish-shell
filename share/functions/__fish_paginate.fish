function __fish_paginate -d "Paginate the current command using the users default pager"

	set -l cmd less
	if set -q PAGER
		set cmd $PAGER
	end

	if commandline -j| string match -q -r -v "$cmd *\$"

		commandline -aj " ^&1 |$cmd;"
	end

end
