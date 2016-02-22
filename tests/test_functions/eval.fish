function eval -S -d "Evaluate parameters as a command"
  # keep a copy of the previous $status and use restore_status
  # to preserve the status in case the block that is evaluated
  # does not modify the status itself.
  set -l status_copy $status
  function -S restore_status
    return $status_copy
  end

  if not set -q argv[2]
    # like most builtins, we only check for -h/--help
    # if we only have a single argument
    switch "$argv[1]"
    case -h --help
      __fish_print_help eval
      return 0
    end
  end

  # If we are in an interactive shell, eval should enable full
  # job control since it should behave like the real code was
  # executed. If we don't do this, commands that expect to be
  # used interactively, like less, wont work using eval.

  set -l mode
  if status --is-interactive-job-control
    set mode interactive
  else
    if status --is-full-job-control
      set mode full
    else
      set mode none
    end
  end
  if status --is-interactive
    status --job-control full
  end

  # rfish: To eval 'foo', we construct a block "begin ; foo; end <&3 3<&-"
  # The 'eval2_inner' is a param to 'begin' itself; I believe it does nothing.
  # Note the redirections are also within the quotes.
  #
  # We then pipe this to 'source 3<&0' which dup2's 3 to stdin.
  #
  # You might expect that the dup2(3, stdin) should overwrite stdin,
  # and therefore prevent 'source' from reading the piped-in block. This doesn't
  # happen because when you pipe to a builtin, we don't overwrite stdin with the
  # read end of the block; instead we set a separate fd in a variable 'builtin_stdin',
  # which is what it reads from. So builtins are magic in that, in pipes, their stdin
  # is not fd 0.

  restore_status
  echo "begin; $argv "\n" ;end eval2_inner <&3 3<&-" | source 3<&0
  set -l res $status

  status --job-control $mode
  functions -e restore_status

  return $res
end
