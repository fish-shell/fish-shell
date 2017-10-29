function grepify --description Adds\ \'grep\'\ to\ a\ command
	set -l cmd less
  set cmd "grep ''"

  if test -z (commandline -j)
    commandline -a $history[1]
  end

  if commandline -j | string match -q -r -v "$cmd *\$"
    commandline -aj " ^&1 | $cmd;"

    commandline -C (math (string length (commandline -j)) - 1)
  end
end
