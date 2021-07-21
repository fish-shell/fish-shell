function __fish_az_complete
  set -lx _ARGCOMPLETE 1
  set -lx _ARGCOMPLETE_IFS \n
  set -lx _ARGCOMPLETE_SUPPRESS_SPACE 1
  set -lx _ARGCOMPLETE_SHELL fish
  set -lx COMP_LINE (commandline -pc)
  set -lx COMP_POINT (string length (commandline -cp))
  set -lx COMP_TYPE
  az 8>&1 9>&2 2>/dev/null
end

complete -c az -f -a '(__fish_az_complete)'
