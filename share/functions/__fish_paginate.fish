function __fish_paginate -d "Paginate the current command using the users default pager"

	set -l cmd less
	if set -q PAGER
		set cmd $PAGER
	end

	if commandline -j|grep -v "$cmd *\$" >/dev/null

		commandline -aj " ^&1 |$cmd;"
	end

end
