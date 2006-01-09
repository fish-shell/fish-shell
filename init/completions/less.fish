complete -c less -s \? -l help -d (_ "Display help and exit")
complete -c less -s a -l search-skip-screen -d (_ "Search after end of screen")
complete -c less -s b -l buffers -r -d (_ "Buffer space")
complete -c less -s B -l auto-buffers -d (_ "Disable automtic buffer allocation")
complete -c less -s c -l clear-screen -d (_ "Repaint from top")
complete -c less -s C -l CLEAR-SCREEN -d (_ "Clear and repaint from top")
complete -c less -s d -l dumb -d (_ "Supress error for lacking terminal capability")
complete -c less -s e -l quit-at-eof -d (_ "Exit on second EOF")
complete -c less -s E -l QUIT-AT-EOF -d (_ "Exit on EOF")
complete -c less -s f -l force -d (_ "Open non-regular files")
complete -c less -s F -l quit-if-one-screen -d (_ "Quit if file shorter than one screen")
complete -c less -s g -l hilite-search -d (_ "Hilight one search target")
complete -c less -s G -l HILITE-SEARCH -d (_ "No search highlighting")
complete -c less -s h -l max-back-scroll -d (_ "Maximum backward scroll") -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"
complete -c less -s i -l ignore-case -d (_ "Search ignores lowercase case")
complete -c less -s I -l IGNORE-CASE -d (_ "Search ignores all case")
complete -c less -s j -l jump-target -d (_ "Target line") -r -a "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -13 -14 -15 -16 -17 -18 -19"
complete -c less -s J -l status-column -d (_ "Display status column")
complete -c less -s k -l lesskey-file -d (_ "Specify key bindings file") -r
complete -c less -s L -l no-lessopen -d 'Ignore $LESSOPEN'
complete -c less -s m -l long-prompt -d (_ "Prompt with percentage")
complete -c less -s M -l LONG-PROMPT -d (_ "Verbose prompt")
complete -c less -s n -l line-numbers -d (_ "Display line number")
complete -c less -s N -l LINE-NUMBERS -d (_ "Display line number for each line")
complete -c less -s o -l log-file -d (_ "Log input to file") -r
complete -c less -s O -l LOG-FILE -d (_ "Log to file, overwrite") -r
complete -c less -s p -l pattern -d (_ "Start at first occorance of pattern") -r
complete -c less -s P -l prompt -d (_ "Prompt string") -r
complete -c less -s q -l quiet -d (_ "Silent mode")
complete -c less -l silent -d (_ "Silent mode")
complete -c less -s Q -l QUIET -d (_ "Completly silent mode")
complete -c less -l SILENT -d (_ "Completly silent mode")
complete -c less -s r -l raw-control-chars -d (_ "Display control chars")
complete -c less -s R -l RAW-CONTROL-CHARS -d (_ "Display control chars, guess screen appearance")
complete -c less -s s -l squeeze-blank-lines -d (_ "Multiple blank lines sqeezed")
complete -c less -s S -l chop-long-lines -d (_ "Do not fold long lines")
complete -c less -s t -l tag -d (_ "Edit tag") -r
complete -c less -s T -l tag-file -d (_ "Set tag file") -r
complete -c less -s u -l underline-special -d (_ "Allow backspace and carriage return")
complete -c less -s U -l UNDERLINE-SPECIAL -d (_ "Allow backspace, tab and carriage return")
complete -c less -s V -l version -d (_ "Display version and exit")
complete -c less -s w -l hilite-unread -d (_ "Highlight first unread line on new page")
complete -c less -s W -l HILITE-UNREAD -d (_ "Highlight first unread line on any movement")
complete -c less -s x -l tabs -d (_ "Set tab stops") -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16"
complete -c less -s X -l no-init -d (_ "No termcap init")
complete -c less -l no-keypad -d (_ "No keypad init")
complete -c less -s y -l max-forw-scroll -d (_ "Maximum forward scroll") -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"
complete -c less -s z -l window -d (_ "Max scroll window") -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"
complete -c less -s \" -l quotes -d (_ "Set quote char") -r
complete -c less -s \~ -l tilde -d (_ "Lines after EOF are blank")
complete -c less -s \# -l shift -d (_ "Characters to scroll on left/right arrows") -a "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"

