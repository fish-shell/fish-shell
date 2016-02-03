function __fish_cursor_xterm -d 'Set cursor (xterm)'
  set -l shape $argv[1]

  switch "$shape"
    case block
      set shape 2
    case underscore
      set shape 4
    case line
      set shape 6
  end
  if contains blink $argv
    set shape (math $shape - 1)
  end
  echo -en "\e[$shape q"
end
