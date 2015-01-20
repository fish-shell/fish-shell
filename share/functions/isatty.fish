function isatty -d "Test if a file or file descriptor is a tty."

# Use `command test` because `builtin test` doesn't open the regular fd's.

  switch "$argv"

    case '-h*' '--h*'
      __fish_print_help isatty

    case '' '-'
      tty 0 2>&1 >/dev/null
      return $status

    case 0 1 2
      tty $argv 2>&1 >/dev/null
      return $status

    case '*'
      if test -e "$argv"
        command test -c "$argv"
          and tty 0>"$argv" 2>&1 >/dev/null
        return $status

      else if test -e /dev/"$argv"
        command test -c /dev/"$argv"
          and tty 0>/dev/"$argv" 2>&1 >/dev/null
       return $status

      else if test -e /dev/fd/"$argv"
        command test -c /dev/fd/"$argv"
          and tty 0>/dev/fd/"$argv" 2>&1 >/dev/null
        return $status

      else
         return 1
    end
  end
end
