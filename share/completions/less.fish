complete -c less -s \? -l help --description "Display help and exit"
complete -c less -s a -l search-skip-screen --description "Search after end of screen"
complete -c less -s b -l buffers -r --description "Buffer space"
complete -c less -s B -l auto-buffers --description "Disable automtic buffer allocation"
complete -c less -s c -l clear-screen --description "Repaint from top"
complete -c less -s C -l CLEAR-SCREEN --description "Clear and repaint from top"
complete -c less -s d -l dumb --description "Supress error for lacking terminal capability"
complete -c less -s e -l quit-at-eof --description "Exit on second EOF"
complete -c less -s E -l QUIT-AT-EOF --description "Exit on EOF"
complete -c less -s f -l force --description "Open non-regular files"
complete -c less -s F -l quit-if-one-screen --description "Quit if file shorter than one screen"
complete -c less -s g -l hilite-search --description "Hilight one search target"
complete -c less -s G -l HILITE-SEARCH --description "No search highlighting"
complete -c less -s h -l max-back-scroll --description "Maximum backward scroll" -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"
complete -c less -s i -l ignore-case --description "Search ignores lowercase case"
complete -c less -s I -l IGNORE-CASE --description "Search ignores all case"
complete -c less -s j -l jump-target --description "Target line" -r -a "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -13 -14 -15 -16 -17 -18 -19"
complete -c less -s J -l status-column --description "Display status column"
complete -c less -s k -l lesskey-file --description "Specify key bindings file" -r
complete -c less -s L -l no-lessopen -d 'Ignore $LESSOPEN'
complete -c less -s m -l long-prompt --description "Prompt with percentage"
complete -c less -s M -l LONG-PROMPT --description "Verbose prompt"
complete -c less -s n -l line-numbers --description "Display line number"
complete -c less -s N -l LINE-NUMBERS --description "Display line number for each line"
complete -c less -s o -l log-file --description "Log input to file" -r
complete -c less -s O -l LOG-FILE --description "Log to file, overwrite" -r
complete -c less -s p -l pattern --description "Start at first occurrence of pattern" -r
complete -c less -s P -l prompt --description "Prompt string" -r
complete -c less -s q -l quiet --description "Silent mode"
complete -c less -l silent --description "Silent mode"
complete -c less -s Q -l QUIET --description "Completly silent mode"
complete -c less -l SILENT --description "Completly silent mode"
complete -c less -s r -l raw-control-chars --description "Display control chars"
complete -c less -s R -l RAW-CONTROL-CHARS --description "Display control chars, guess screen appearance"
complete -c less -s s -l squeeze-blank-lines --description "Multiple blank lines sqeezed"
complete -c less -s S -l chop-long-lines --description "Do not fold long lines"
complete -c less -s t -l tag --description "Edit tag" -r
complete -c less -s T -l tag-file --description "Set tag file" -r
complete -c less -s u -l underline-special --description "Allow backspace and carriage return"
complete -c less -s U -l UNDERLINE-SPECIAL --description "Allow backspace, tab and carriage return"
complete -c less -s V -l version --description "Display version and exit"
complete -c less -s w -l hilite-unread --description "Highlight first unread line on new page"
complete -c less -s W -l HILITE-UNREAD --description "Highlight first unread line on any movement"
complete -c less -s x -l tabs --description "Set tab stops" -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16"
complete -c less -s X -l no-init --description "No termcap init"
complete -c less -l no-keypad --description "No keypad init"
complete -c less -s y -l max-forw-scroll --description "Maximum forward scroll" -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"
complete -c less -s z -l window --description "Max scroll window" -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"
complete -c less -s \" -l quotes --description "Set quote char" -r
complete -c less -s \~ -l tilde --description "Lines after EOF are blank"
complete -c less -s \# -l shift --description "Characters to scroll on left/right arrows" -a "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"

