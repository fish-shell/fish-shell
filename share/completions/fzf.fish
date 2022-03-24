complete -c fzf -f

# Search mode
complete -c fzf -l no-extended -d no-extended
complete -c fzf -n 'string match "+*" -- (commandline -ct)' -a +x -d no-extended
complete -c fzf -s e -l exact -d 'Enable exact-match'
complete -c fzf -n 'string match "+*" -- (commandline -ct)' -a +i -d 'case-sensitive match'
complete -c fzf -s i -d 'Case-insensitive match'
complete -c fzf -l literal -d 'Do not normalize latin script letters for matching'
complete -c fzf -l algo -d 'Fuzzy matching algorithm' -x -a 'v1 v2'
complete -c fzf -s n -l nth -d 'Limit search scope using field index expressions' -x
complete -c fzf -l with-nth -d 'Transform each line using field index expressions' -x
complete -c fzf -s d -l delimiter -d 'Field delimiter regex for --nth and --with-nth' -x

# Search result
complete -c fzf -l no-sort -d 'Do not sort the result'
complete -c fzf -n 'string match "+*" -- (commandline -ct)' -a +s -d 'Do not sort the result'
complete -c fzf -l tac -d 'Reverse the order of the input'
complete -c fzf -l disabled -d 'Do not perform search'
complete -c fzf -l tiebreak -d 'Sort criteria when breaking ties' -x -a 'length begin end index'

# Interface
complete -c fzf -s m -l multi -d 'Enable multi-select with tab/shift-tab'
complete -c fzf -l no-multi -d 'Disable multi-select'
complete -c fzf -n 'string match "+*" -- (commandline -ct)' -a +m -d 'Disable multi-select'
complete -c fzf -l no-mouse -d 'Disable mouse'
complete -c fzf -l bind -d 'Custom key bindings' -x
complete -c fzf -l cycle -d 'Enable cyclic scroll'
complete -c fzf -l keep-right -d 'Keep the right end of the line visible on overflow'
complete -c fzf -l no-hscroll -d 'Disable horizontal scroll'
complete -c fzf -l hscroll-off -d 'Number of columns to keep right of highlighted substring' -x
complete -c fzf -l filepath-word -d 'Make word-wise movements respect path separators'
complete -c fzf -l jump-labels -d 'Label characters for jump and jump-accept' -x

# layout
complete -c fzf -l height -d 'Display fzf window with the given height' -x
complete -c fzf -l min-height -d 'Minimum height when --height is given in percent' -x
complete -c fzf -l layout -d 'Choose the layout' -x -a 'default reverse reverse-list'
complete -c fzf -l reverse -d 'A synonym for --layout=reverse'
complete -c fzf -l border -d 'Draw border around the finder' -x -a 'rounded sharp horizontal'
complete -c fzf -l no-unicode -d 'Draw border with ASCII characters'
complete -c fzf -l margin -d 'Expression for margins around the finder'
complete -c fzf -l info -d 'Finder info style' -x -a 'default inline hidden'
complete -c fzf -l inline-info -d 'Display finder info inline with the query'
complete -c fzf -l prompt -d 'Input prompt' -x
complete -c fzf -l pointer -d 'Pointer to the current line' -x
complete -c fzf -l marker -d 'Multi-select marker' -x
complete -c fzf -l header -d 'String to print as header' -x
complete -c fzf -l header-lines -d 'Treat the first N lines of input as header' -x

# Display
complete -c fzf -l ansi -d 'Enable processing of ANSI color codes'
complete -c fzf -l tabstop -d 'Number of spaces for a tab character' -x
complete -c fzf -l color -d 'Color configuration' -x
complete -c fzf -l no-bold -d 'Do not use bold text'
complete -c fzf -l black -d 'Use black background'

# History
complete -c fzf -l history -d 'History file' -x
complete -c fzf -l history-size -d 'Maximum number of history entries' -x

# Preview
complete -c fzf -l preview -d 'Command to preview highlighted line' -x
complete -c fzf -l preview-window -d 'Preview window layout' -x

# Scripting
complete -c fzf -s q -l query -d 'Start the finder with the given query' -x
complete -c fzf -s 1 -l select-1 -d 'Automatically select the only match'
complete -c fzf -s 0 -l exit-0 -d 'Exit immediately when there\'s no match'
complete -c fzf -s f -l filter -d 'Filter mode. Do not start interactive finder' -x
complete -c fzf -l print-query -d 'Print query as the first line'
complete -c fzf -l expect -d 'List of keys to complete fzf' -x
complete -c fzf -l read0 -d 'Read input delimited by ASCII NUL characters'
complete -c fzf -l print0 -d 'Print output delimited by ASCII NUL characters'
complete -c fzf -l no-clear -d 'Do not clear finder interface on exit'
complete -c fzf -l sync -d 'Synchronous search for multi-staged filtering'
complete -c fzf -l version -d 'Display version information and exit'
