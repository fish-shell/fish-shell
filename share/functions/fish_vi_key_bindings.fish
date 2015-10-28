function fish_vi_key_bindings --description 'vi-like key bindings for fish'
  bind --erase --all
  set -l init_mode insert
  if set -q argv[1]
    set init_mode $argv[1]
  end

  # Inherit default key bindings
  # Do this first so vi-bindings win over default
  fish_default_key_bindings -M insert
  fish_default_key_bindings -M default

  # Add a way to get out of insert mode
  bind -M insert -m default \cc force-repaint
  bind -M insert -m default \e backward-char force-repaint

  ##
  ## command mode
  ##

  bind :q exit

  #
  # normal (default) mode
  #

  bind \cd exit
  bind \cc 'commandline ""'
  bind h backward-char
  bind l forward-char
  bind \e\[C forward-char
  bind \e\[D backward-char

  # Some linux VTs output these (why?)
  bind \eOC forward-char
  bind \eOD backward-char

  bind -k right forward-char
  bind -k left backward-char
  bind -m insert \n execute
  bind -m insert i force-repaint
  bind -m insert I beginning-of-line force-repaint
  bind -m insert a forward-char force-repaint
  bind -m insert A end-of-line force-repaint
  bind -m visual v begin-selection force-repaint

  #bind -m insert o "commandline -a \n" down-line force-repaint
  #bind -m insert O beginning-of-line "commandline -i \n" up-line force-repaint # doesn't work

  bind gg beginning-of-buffer
  bind G end-of-buffer

  bind \x24 end-of-line
  bind \x5e beginning-of-line
  bind 0 beginning-of-line
  bind g\x24 end-of-line
  bind g\x5e beginning-of-line
  bind \e\[H beginning-of-line
  bind \e\[F end-of-line

  bind u history-search-backward
  bind \cr history-search-forward

  bind [ history-token-search-backward
  bind ] history-token-search-forward

  bind k up-or-search
  bind j down-or-search
  bind \e\[A up-or-search
  bind \e\[B down-or-search
  bind -k down down-or-search
  bind -k up up-or-search
  bind \eOA up-or-search
  bind \eOB down-or-search

  bind b backward-word
  bind B backward-bigword
  bind ge backward-word
  bind gE backward-bigword
  bind w forward-word
  bind W forward-bigword
  bind e forward-word
  bind E forward-bigword

  bind x delete-char
  bind X backward-delete-char

  bind -k dc delete-char

  bind -k backspace backward-delete-char
  bind \x7f backward-delete-char
  bind \e\[3\;2~ backward-delete-char # Mavericks Terminal.app shift-delete

  bind dd kill-whole-line
  bind D kill-line
  bind d\x24 kill-line
  bind d\x5e backward-kill-line
  bind dw kill-word
  bind dW kill-bigword
  bind diw forward-char forward-char backward-word kill-word
  bind diW forward-char forward-char backward-bigword kill-bigword
  bind daw forward-char forward-char backward-word kill-word
  bind daW forward-char forward-char backward-bigword kill-bigword
  bind de kill-word
  bind dE kill-bigword
  bind db backward-kill-word
  bind dB backward-kill-bigword
  bind dge backward-kill-word
  bind dgE backward-kill-bigword

  bind -m insert s delete-char force-repaint
  bind -m insert S kill-whole-line force-repaint
  bind -m insert cc kill-whole-line force-repaint
  bind -m insert C kill-line force-repaint
  bind -m insert c\x24 kill-line force-repaint
  bind -m insert c\x5e backward-kill-line force-repaint
  bind -m insert cw kill-word force-repaint
  bind -m insert cW kill-bigword force-repaint
  bind -m insert ciw forward-char forward-char backward-word kill-word force-repaint
  bind -m insert ciW forward-char forward-char backward-bigword kill-bigword force-repaint
  bind -m insert caw forward-char forward-char backward-word kill-word force-repaint
  bind -m insert caW forward-char forward-char backward-bigword kill-bigword force-repaint
  bind -m insert ce kill-word force-repaint
  bind -m insert cE kill-bigword force-repaint
  bind -m insert cb backward-kill-word force-repaint
  bind -m insert cB backward-kill-bigword force-repaint
  bind -m insert cge backward-kill-word force-repaint
  bind -m insert cgE backward-kill-bigword force-repaint

  bind '~' capitalize-word 
  bind gu downcase-word 
  bind gU upcase-word 

  bind J end-of-line delete-char
  bind K 'man (commandline -t) ^/dev/null; or echo -n \a'

  bind yy kill-whole-line yank
  bind Y  kill-whole-line yank
  bind y\x24 kill-line yank
  bind y\x5e backward-kill-line yank
  bind yw kill-word yank
  bind yW kill-bigword yank
  bind yiw forward-char forward-char backward-word kill-word yank
  bind yiW forward-char forward-char backward-bigword kill-bigword yank
  bind yaw forward-char forward-char backward-word kill-word yank
  bind yaW forward-char forward-char backward-bigword kill-bigword yank
  bind ye kill-word yank
  bind yE kill-bigword yank
  bind yb backward-kill-word yank
  bind yB backward-kill-bigword yank
  bind yge backward-kill-word yank
  bind ygE backward-kill-bigword yank

  bind f forward-jump
  bind F backward-jump
  bind t forward-jump and backward-char
  bind T backward-jump and forward-char

  # in emacs yank means paste
  bind p yank
  bind P backward-char yank
  bind gp yank-pop

  ### Overrides
  # This is complete in vim
  bind -M insert \cx end-of-line

  bind -M insert \cf forward-word
  bind -M insert \cb backward-word

  bind '"*p' "commandline -i ( xsel -p; echo )[1]"
  bind '"*P' backward-char "commandline -i ( xsel -p; echo )[1]"

  #
  # Lowercase r, enters replace-one mode
  #

  bind -m replace-one r force-repaint
  bind -M replace-one -m default '' delete-char self-insert backward-char force-repaint
  bind -M replace-one -m default \e cancel force-repaint

  #
  # visual mode
  #

  bind -M visual \e\[C forward-char
  bind -M visual \e\[D backward-char
  bind -M visual -k right forward-char
  bind -M visual -k left backward-char
  bind -M insert \eOC forward-char
  bind -M insert \eOD backward-char
  bind -M visual h backward-char
  bind -M visual l forward-char

  bind -M visual b backward-word
  bind -M visual B backward-bigword
  bind -M visual ge backward-word
  bind -M visual gE backward-bigword
  bind -M visual w forward-word
  bind -M visual W forward-bigword
  bind -M visual e forward-word
  bind -M visual E forward-bigword

  bind -M visual -m default d kill-selection end-selection force-repaint
  bind -M visual -m default x kill-selection end-selection force-repaint
  bind -M visual -m default X kill-whole-line end-selection force-repaint
  bind -M visual -m default y kill-selection yank end-selection force-repaint
  bind -M visual -m default '"*y' "commandline -s | xsel -p" end-selection force-repaint

  bind -M visual -m default \cc end-selection force-repaint
  bind -M visual -m default \e  end-selection force-repaint

  set fish_bind_mode $init_mode
end
