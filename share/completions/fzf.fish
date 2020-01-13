complete -c fzf -f

# Search mode
complete -c fzf -l no-extended -d 'no-extended'
complete -c fzf -n 'string match "+*" -- (commandline -ct)' -a +x -d 'no-extended'
complete -c fzf -s e -l --exact -d 'Enable exact-match'
complete -c fzf -n 'string match "+*" -- (commandline -ct)' -a +i -d 'case-sensitive match'
complete -c fzf -s i -d 'Case-insensitive match'
complete -c fzf -l literal -d 'Do not normalize latin script letters for matching'
complete -c fzf -l algo -d 'Fuzzy matching algorithm' -x -a 'v1 v2'
complete -c fzf -s n -l nth -d 'Comma-separated list of field index expressions for limiting search scope' -x
complete -c fzf -l with-nth -d 'Transform the presentation of each line using field index expressions' -x
complete -c fzf -s d -l delimiter -d 'Field delimiter regex for --nth and --with-nth' -x

# Search result
complete -c fzf -l no-sort -d 'Do not sort the result'
complete -c fzf -n 'string match "+*" -- (commandline -ct)' -a +s -d 'Do not sort the result'
complete -c fzf -l tac -d 'Reverse the order of the input'
complete -c fzf -l tiebreak -d 'Comma-separated list of sort criteria to apply when the scores are tied.' -x

# Interface
complete -c fzf -s m -l multi -d 'Enable multi-select with tab/shift-tab'
complete -c fzf -l no-multi -d 'Disable multi-select'
complete -c fzf -n 'string match "+*" -- (commandline -ct)' -a +m -d 'Disable multi-select'
complete -c fzf -l no-mouse -d 'Disable mouse'
complete -c fzf -l bind -d 'Comma-separated list of custom key bindings' -x
complete -c fzf -l cycle -d 'Enable cyclic scroll'
complete -c fzf -l no-hscroll -d 'Disable horizontal scroll'
complete -c fzf -l hscroll-off -d 'Number of screen columns to keep to the right of the highlighted substring' -x
complete -c fzf -l filepath-word -d 'Make word-wise movements and actions respect path separators'
complete -c fzf -l jump-labels -d 'Make word-wise movements and actions respect path separators' -x

# layout
complete -c fzf -l height -d 'Display fzf window below the cursor with the given height instead of using the full screen' -x
complete -c fzf -l min-height -d 'Minimum height when --height is given in percent (default: 10)' -x
complete -c fzf -l layout -d 'Choose the layout' -x -a 'default reverse reverse-list'
complete -c fzf -l reverse -d 'A synonym for --layout=reverse'
complete -c fzf -l border -d 'Draw border above and below the finder'
complete -c fzf -l no-unicode -d 'Use ASCII characters instead of Unicode box drawing characters to draw border'
complete -c fzf -l margin -d 'Comma-separated expression for margins around the finder'
complete -c fzf -l inline-info -d 'Display finder info inline with the query '
complete -c fzf -l prompt -d 'Input prompt' -x
complete -c fzf -l header -d 'The  given  string  will be printed as the sticky header' -x
complete -c fzf -l header-lines -d 'The first N lines of the input are treated as the sticky header' -x

# Display
complete -c fzf -l ansi -d 'Enable processing of ANSI color codes'
complete -c fzf -l tabstop -d 'Number of spaces for a tab character (default: 8)'
complete -c fzf -l color -d 'Color configuration' -x
complete -c fzf -l no-bold -d 'Do not use bold text'
complete -c fzf -l black -d 'Use black background'

# History
complete -c fzf -l history -d 'Load search history from the specified file and update the file on completion' -x
complete -c fzf -l history-size -d 'Maximum number of entries in the history file' -x

# Preview
complete -c fzf -l preview -d 'Execute the given command for the current line and display the result on the preview window' -x
complete -c fzf -l preview-window -d 'Determine the layout of the preview window' -x

# Scripting
complete -c fzf -s q -l query -d 'Start the finder with the given query' -x
complete -c fzf -s 1 -l select-1 -d 'Automatically select the only match'
complete -c fzf -s 0 -l exit-0 -d 'Exit immediately when there\'s no match'
complete -c fzf -s f -l filter -d 'Filter mode. Do not start interactive finder' -x
complete -c fzf -l print-query -d 'Print query as the first line'
complete -c fzf -l expect -d 'Comma-separated list of keys that can be used to complete fzf in addition to the default enter key' -x
complete -c fzf -l read0 -d 'Read input delimited by ASCII NUL characters instead of newline characters'
complete -c fzf -l print0 -d 'Print output delimited by ASCII NUL characters instead of newline characters'
complete -c fzf -l no-clear -d 'Do not clear finder interface on exit'
complete -c fzf -l sync -d 'Synchronous search for multi-staged filtering'
complete -c fzf -l version -d 'Display version information and exit'
