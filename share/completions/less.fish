complete -c less -s \? -l help -d (N_ "Display help and exit")
complete -c less -s a -l search-skip-screen -d (N_ "Search after end of screen")
complete -c less -s b -l buffers -r -d (N_ "Buffer space")
complete -c less -s B -l auto-buffers -d (N_ "Disable automtic buffer allocation")
complete -c less -s c -l clear-screen -d (N_ "Repaint from top")
complete -c less -s C -l CLEAR-SCREEN -d (N_ "Clear and repaint from top")
complete -c less -s d -l dumb -d (N_ "Supress error for lacking terminal capability")
complete -c less -s e -l quit-at-eof -d (N_ "Exit on second EOF")
complete -c less -s E -l QUIT-AT-EOF -d (N_ "Exit on EOF")
complete -c less -s f -l force -d (N_ "Open non-regular files")
complete -c less -s F -l quit-if-one-screen -d (N_ "Quit if file shorter than one screen")
complete -c less -s g -l hilite-search -d (N_ "Hilight one search target")
complete -c less -s G -l HILITE-SEARCH -d (N_ "No search highlighting")
complete -c less -s h -l max-back-scroll -d (N_ "Maximum backward scroll") -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"
complete -c less -s i -l ignore-case -d (N_ "Search ignores lowercase case")
complete -c less -s I -l IGNORE-CASE -d (N_ "Search ignores all case")
complete -c less -s j -l jump-target -d (N_ "Target line") -r -a "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -13 -14 -15 -16 -17 -18 -19"
complete -c less -s J -l status-column -d (N_ "Display status column")
complete -c less -s k -l lesskey-file -d (N_ "Specify key bindings file") -r
complete -c less -s L -l no-lessopen -d 'Ignore $LESSOPEN'
complete -c less -s m -l long-prompt -d (N_ "Prompt with percentage")
complete -c less -s M -l LONG-PROMPT -d (N_ "Verbose prompt")
complete -c less -s n -l line-numbers -d (N_ "Display line number")
complete -c less -s N -l LINE-NUMBERS -d (N_ "Display line number for each line")
complete -c less -s o -l log-file -d (N_ "Log input to file") -r
complete -c less -s O -l LOG-FILE -d (N_ "Log to file, overwrite") -r
complete -c less -s p -l pattern -d (N_ "Start at first occurrence of pattern") -r
complete -c less -s P -l prompt -d (N_ "Prompt string") -r
complete -c less -s q -l quiet -d (N_ "Silent mode")
complete -c less -l silent -d (N_ "Silent mode")
complete -c less -s Q -l QUIET -d (N_ "Completly silent mode")
complete -c less -l SILENT -d (N_ "Completly silent mode")
complete -c less -s r -l raw-control-chars -d (N_ "Display control chars")
complete -c less -s R -l RAW-CONTROL-CHARS -d (N_ "Display control chars, guess screen appearance")
complete -c less -s s -l squeeze-blank-lines -d (N_ "Multiple blank lines sqeezed")
complete -c less -s S -l chop-long-lines -d (N_ "Do not fold long lines")
complete -c less -s t -l tag -d (N_ "Edit tag") -r
complete -c less -s T -l tag-file -d (N_ "Set tag file") -r
complete -c less -s u -l underline-special -d (N_ "Allow backspace and carriage return")
complete -c less -s U -l UNDERLINE-SPECIAL -d (N_ "Allow backspace, tab and carriage return")
complete -c less -s V -l version -d (N_ "Display version and exit")
complete -c less -s w -l hilite-unread -d (N_ "Highlight first unread line on new page")
complete -c less -s W -l HILITE-UNREAD -d (N_ "Highlight first unread line on any movement")
complete -c less -s x -l tabs -d (N_ "Set tab stops") -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16"
complete -c less -s X -l no-init -d (N_ "No termcap init")
complete -c less -l no-keypad -d (N_ "No keypad init")
complete -c less -s y -l max-forw-scroll -d (N_ "Maximum forward scroll") -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"
complete -c less -s z -l window -d (N_ "Max scroll window") -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"
complete -c less -s \" -l quotes -d (N_ "Set quote char") -r
complete -c less -s \~ -l tilde -d (N_ "Lines after EOF are blank")
complete -c less -s \# -l shift -d (N_ "Characters to scroll on left/right arrows") -a "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"

