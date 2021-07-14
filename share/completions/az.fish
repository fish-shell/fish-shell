function __fish_az_complete
  set -x _ARGCOMPLETE 1
  set -x _ARGCOMPLETE_IFS \n
  set -x _ARGCOMPLETE_SUPPRESS_SPACE 1
  set -x _ARGCOMPLETE_SHELL fish
  set -x COMP_LINE (commandline -p)
  set -x COMP_POINT (string length (commandline -cp))
  set -x COMP_TYPE
  if set -q _ARC_DEBUG
    az 8>&1 9>&2 1>/dev/null 2>&1
  else
    az 8>&1 9>&2 1>&9 2>&1
  end
end

complete -c az -f -a '(__fish_az_complete)'
