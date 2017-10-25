function __fish_paginate --description 'Paginate the current command using the users default pager'
	set -l cmd less
  if set -q PAGER
    set cmd $PAGER
  end

  if test -z (commandline -j)
    commandline -a $history[1]
  end

  if commandline -j | string match -q -r -v "$cmd *\$"
    commandline -aj " ^&1 | $cmd;"
  else
    commandline -aj "# ^&1 | $cmd;"
  end
end
