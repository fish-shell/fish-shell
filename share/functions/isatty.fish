function isatty -d "Test if a file or file descriptor is a tty."

# Use `command test` because `builtin test` doesn't open the regular fd's.

  switch "$argv"

    case '-h*' '--h*'
      __fish_print_help isatty

    case ''
      command test -c /dev/stdin

    case '*'
      if test -e "$argv" # The eval here is needed for symlinks. Unsure why.
        command test -c "$argv"; and eval tty 0>"$argv" >/dev/null

      else if test -e /dev/"$argv"
         command test -c /dev/"$argv"; and tty 0>/dev/"$argv" >/dev/null

      else if test -e /dev/fd/"$argv"
         command test -c /dev/fd/"$argv"; and tty 0>/dev/fd/"$argv" >/dev/null

      else
         return 1
    end
  end
end
