function fish_vi_cursor -d 'Set cursor shape for different vi modes'
  set -l terminal $argv[1]
  set -q terminal[1]; or set terminal auto

  switch "$terminal"
    case auto
      if set -q KONSOLE_PROFILE_NAME
        set terminal konsole
      else if set -q XTERM_LOCALE
        set terminal xterm
      else
        #echo Not found
        return 1
    end
  end

  set -l command
  set -l start
  set -l end
  set -l shape_block
  set -l shape_line
  set -l shape_underline
  switch "$terminal"
    case konsole iterm
      set command echo -en
      set start "\e]50;"
      set end "\x7"
      set shape_block 'CursorShape=0'
      set shape_line 'CursorShape=1'
      set shape_underline 'CursorShape=2'
    case xterm
      set command echo -en
      set start '\e['
      set end ' q'
      set shape_block '2'
      set shape_underline '4'
      set shape_line '6'
  end
  if not set -q command[1]
    #echo not found
    return 1
  end
  set -g fish_cursor_insert  $start$shape_line$end
  set -g fish_cursor_default $start$shape_block$end
  set -g fish_cursor_other   $start$shape_block$end

  echo "
    function fish_vi_cursor_handle --on-variable fish_bind_mode
      switch \$fish_bind_mode
        case insert
          $command \$fish_cursor_insert
        case default
          $command \$fish_cursor_default
        case '*'
          $command \$fish_cursor_other
      end
    end
      " | source
end
