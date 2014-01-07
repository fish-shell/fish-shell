
function isatty -d "Tests if a file descriptor is a tty"
		switch "$argv"

			case -h --h --he --hel --help
				__fish_print_help isatty
				return 0

			case stdin 0 ''
				set fd 0

			case stdout 1
				set fd 1

			case stderr 2
				set fd 2

		end

  switch "$fd"
    case 0 1 2
	eval "tty 0>&$fd >/dev/null"
       return $status
  end

 # Everything above this comment shouldn't be necessary with the code below, but
 # fish's built-in `test` doesn't properly handle the '-c' flag in a pipe here.

 # If you have a POSIX-compliant /bin/test, drop it in here to see the effect.

  if test -z "$argv"
     test -c /dev/fd/0

  else if test -e "$argv"
       if test -c "$argv"; eval tty 0>"$argv" >/dev/null; end

  else if test -e /dev/"$argv"
       if test -c /dev/"$argv"; eval tty 0>/dev/"$argv" >/dev/null; end

  else if test -e /dev/fd/"$argv"
       if test -c /dev/fd/"$argv"; eval tty 0>/dev/fd/"$argv" >/dev/null; end

  else
    return 1

  end
end
