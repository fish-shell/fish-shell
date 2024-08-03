complete -c less -s \? -l help -d "Display help and exit"
complete -c less -s a -l search-skip-screen -d "Search after end of screen"
complete -c less -s b -l buffers -r -d "Buffer space"
complete -c less -s B -l auto-buffers -d "Disable automtic buffer allocation"
complete -c less -s c -l clear-screen -d "Repaint from top"
complete -c less -s C -l CLEAR-SCREEN -d "Clear and repaint from top"
complete -c less -s d -l dumb -d "Suppress error for lacking terminal capability"
#complete -c less -o Dxcolor -l color -d 'Changes the color of different parts of the displayed text' #Complex rules
complete -c less -s e -l quit-at-eof -d "Exit on second EOF"
complete -c less -s E -l QUIT-AT-EOF -d "Exit on EOF"
complete -c less -s f -l force -d "Open non-regular files"
complete -c less -s F -l quit-if-one-screen -d "Quit if file shorter than one screen"
complete -c less -s g -l hilite-search -d "Hilight one search target"
complete -c less -s G -l HILITE-SEARCH -d "No search highlighting"
complete -c less -s h -l max-back-scroll -d "Maximum backward scroll" -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"
complete -c less -s i -l ignore-case -d "Search ignores lowercase case"
complete -c less -s I -l IGNORE-CASE -d "Search ignores all case"
complete -c less -s j -l jump-target -d "Target line" -r -a "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -13 -14 -15 -16 -17 -18 -19"
complete -c less -s J -l status-column -d "Display status column"
complete -c less -s k -l lesskey-file -d "Specify key bindings binary file" -F -r
complete -c less -l lesskey-src -d "Specify key bindings source file" -F -r
complete -c less -s K -l quit-on-intr -d "Quit with status 2 if interrupt keys pressed"
complete -c less -s L -l no-lessopen -d 'Ignore $LESSOPEN'
complete -c less -s m -l long-prompt -d "Prompt with percentage"
complete -c less -s M -l LONG-PROMPT -d "Verbose prompt"
complete -c less -s n -l line-numbers -d "Display line number"
complete -c less -s N -l LINE-NUMBERS -d "Display line number for each line"
complete -c less -o o -l log-file -d "Log input to file" -r
complete -c less -o O -l LOG-FILE -d "Log to file, overwrite" -r
complete -c less -o p -l pattern -d "Start at first occurrence of pattern" -r
complete -c less -o P -l prompt -d "Prompt string" -r
complete -c less -s q -l quiet -d "Silent mode"
complete -c less -l silent -d "Silent mode"
complete -c less -s Q -l QUIET -d "Completly silent mode"
complete -c less -l SILENT -d "Completly silent mode"
complete -c less -s r -l raw-control-chars -d "Display control chars"
complete -c less -s R -l RAW-CONTROL-CHARS -d "Display control chars, guess screen appearance"
complete -c less -s s -l squeeze-blank-lines -d "Multiple blank lines sqeezed"
complete -c less -s S -l chop-long-lines -d "Do not fold long lines"
complete -c less -o t -l tag -d "Edit tag" -r
complete -c less -o T -l tag-file -d "Set tag file" -r
complete -c less -s u -l underline-special -d "Allow backspace and carriage return"
complete -c less -s U -l UNDERLINE-SPECIAL -d "Allow backspace, tab and carriage return"
complete -c less -s V -l version -d "Display version and exit"
complete -c less -s w -l hilite-unread -d "Highlight first unread line on new page"
complete -c less -s W -l HILITE-UNREAD -d "Highlight first unread line on any movement"
complete -c less -s x -l tabs -d "Set tab stops" -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16"
complete -c less -s X -l no-init -d "No termcap init"
complete -c less -l no-keypad -d "No keypad init"
complete -c less -s y -l max-forw-scroll -d "Maximum forward scroll" -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"
complete -c less -o z -l window -d "Max scroll window" -r -a "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"
complete -c less -s \" -l quotes -d "Set quote char" -r
complete -c less -s \~ -l tilde -d "Lines after EOF are blank"
complete -c less -s \# -l shift -d "Characters to scroll on left/right arrows" -a "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19"
complete -c less -l file-size -d "Determine the size of the file immediately after opening"
complete -c less -l follow-name -d "Periodically reopen file by name"
complete -c less -l incsearch -d 'Incremental search commands'
complete -c less -l line-num-width -d 'Set minimum width of line number field'
complete -c less -l mouse -d 'Enables mouse input'
complete -c less -l MOUSE -d 'Enables mouse input with reversed scroll direction'
complete -c less -l no-histdups -d 'No duplicates in search history'
complete -c less -l rscroll -d 'Changes the character used to mark truncated lines'
complete -c less -l save-marks -d 'Save marks in the history file'
complete -c less -l status-col-width -d 'Sets the width of the status column when the -J option is in effect'
complete -c less -l use-backslash -d 'Escape characters with backslashes in the options string'
complete -c less -l use-color -d 'Enables colored text'
complete -c less -l wheel-lines -d 'Number of lines scrolled by mouse wheel'
complete -c less -l quotes -d 'Changes the filename quoting character'
