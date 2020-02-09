# The following ripgrep (rg) completions were generated from the rg sources via
# clap's completion file generator, from the MIT-licensed ripgrep 0.10.0 release.
# To avoid licensing confusion, this file is released under the MIT license.

complete -c rg -n "__fish_use_subcommand" -s A -l after-context -d 'Show NUM lines after each match.'
complete -c rg -n "__fish_use_subcommand" -s B -l before-context -d 'Show NUM lines before each match.'
complete -c rg -n "__fish_use_subcommand" -l color -d 'Controls when to use color.' -r -f -a "never auto always ansi"
complete -c rg -n "__fish_use_subcommand" -l colors -d 'Configure color settings and styles.'
complete -c rg -n "__fish_use_subcommand" -s C -l context -d 'Show NUM lines before and after each match.'
complete -c rg -n "__fish_use_subcommand" -l context-separator -d 'Set the context separator string.'
complete -c rg -n "__fish_use_subcommand" -l dfa-size-limit -d 'The upper size limit of the regex DFA.'
complete -c rg -n "__fish_use_subcommand" -s E -l encoding -d 'Specify the text encoding of files to search.'
complete -c rg -n "__fish_use_subcommand" -s f -l file -d 'Search for patterns from the given file.'
complete -c rg -n "__fish_use_subcommand" -s g -l glob -d 'Include or exclude files.'
complete -c rg -n "__fish_use_subcommand" -l iglob -d 'Include or exclude files case insensitively.'
complete -c rg -n "__fish_use_subcommand" -l ignore-file -d 'Specify additional ignore files.'
complete -c rg -n "__fish_use_subcommand" -s M -l max-columns -d 'Don\'t print lines longer than this limit.'
complete -c rg -n "__fish_use_subcommand" -s m -l max-count -d 'Limit the number of matches.'
complete -c rg -n "__fish_use_subcommand" -l max-depth -d 'Descend at most NUM directories.'
complete -c rg -n "__fish_use_subcommand" -l max-filesize -d 'Ignore files larger than NUM in size.'
complete -c rg -n "__fish_use_subcommand" -l path-separator -d 'Set the path separator.'
complete -c rg -n "__fish_use_subcommand" -l pre -d 'search outputs of COMMAND FILE for each FILE'
complete -c rg -n "__fish_use_subcommand" -l pre-glob -d 'Include or exclude files from a preprocessing command.'
complete -c rg -n "__fish_use_subcommand" -l regex-size-limit -d 'The upper size limit of the compiled regex.'
complete -c rg -n "__fish_use_subcommand" -s e -l regexp -d 'A pattern to search for.'
complete -c rg -n "__fish_use_subcommand" -s r -l replace -d 'Replace matches with the given text.'
complete -c rg -n "__fish_use_subcommand" -l sort -d 'Sort results in ascending order. Implies --threads=1.' -r -f -a "path modified accessed created none"
complete -c rg -n "__fish_use_subcommand" -l sortr -d 'Sort results in descending order. Implies --threads=1.' -r -f -a "path modified accessed created none"
complete -c rg -n "__fish_use_subcommand" -s j -l threads -d 'The approximate number of threads to use.'
complete -c rg -n "__fish_use_subcommand" -s t -l type -d 'Only search files matching TYPE.'
complete -c rg -n "__fish_use_subcommand" -l type-add -d 'Add a new glob for a file type.'
complete -c rg -n "__fish_use_subcommand" -l type-clear -d 'Clear globs for a file type.'
complete -c rg -n "__fish_use_subcommand" -s T -l type-not -d 'Do not search files matching TYPE.'
complete -c rg -n "__fish_use_subcommand" -l block-buffered -d 'Force block buffering.'
complete -c rg -n "__fish_use_subcommand" -l no-block-buffered
complete -c rg -n "__fish_use_subcommand" -s b -l byte-offset -d 'Print the 0-based byte offset for each matching line.'
complete -c rg -n "__fish_use_subcommand" -s s -l case-sensitive -d 'Search case sensitively (default).'
complete -c rg -n "__fish_use_subcommand" -l column -d 'Show column numbers.'
complete -c rg -n "__fish_use_subcommand" -l no-column
complete -c rg -n "__fish_use_subcommand" -s c -l count -d 'Only show the count of matching lines for each file.'
complete -c rg -n "__fish_use_subcommand" -l count-matches -d 'Only show the count of individual matches for each file.'
complete -c rg -n "__fish_use_subcommand" -l crlf -d 'Support CRLF line terminators (useful on Windows).'
complete -c rg -n "__fish_use_subcommand" -l no-crlf
complete -c rg -n "__fish_use_subcommand" -l debug -d 'Show debug messages.'
complete -c rg -n "__fish_use_subcommand" -l trace
complete -c rg -n "__fish_use_subcommand" -l no-encoding
complete -c rg -n "__fish_use_subcommand" -l files -d 'Print each file that would be searched.'
complete -c rg -n "__fish_use_subcommand" -s l -l files-with-matches -d 'Only print the paths with at least one match.'
complete -c rg -n "__fish_use_subcommand" -l files-without-match -d 'Only print the paths that contain zero matches.'
complete -c rg -n "__fish_use_subcommand" -s F -l fixed-strings -d 'Treat the pattern as a literal string.'
complete -c rg -n "__fish_use_subcommand" -l no-fixed-strings
complete -c rg -n "__fish_use_subcommand" -s L -l follow -d 'Follow symbolic links.'
complete -c rg -n "__fish_use_subcommand" -l no-follow
complete -c rg -n "__fish_use_subcommand" -l heading -d 'Print matches grouped by each file.'
complete -c rg -n "__fish_use_subcommand" -l no-heading -d 'Don\'t group matches by each file.'
complete -c rg -n "__fish_use_subcommand" -l hidden -d 'Search hidden files and directories.'
complete -c rg -n "__fish_use_subcommand" -l no-hidden
complete -c rg -n "__fish_use_subcommand" -s i -l ignore-case -d 'Case insensitive search.'
complete -c rg -n "__fish_use_subcommand" -s v -l invert-match -d 'Invert matching.'
complete -c rg -n "__fish_use_subcommand" -l json -d 'Show search results in a JSON Lines format.'
complete -c rg -n "__fish_use_subcommand" -l no-json
complete -c rg -n "__fish_use_subcommand" -l line-buffered -d 'Force line buffering.'
complete -c rg -n "__fish_use_subcommand" -l no-line-buffered
complete -c rg -n "__fish_use_subcommand" -s n -l line-number -d 'Show line numbers.'
complete -c rg -n "__fish_use_subcommand" -s N -l no-line-number -d 'Suppress line numbers.'
complete -c rg -n "__fish_use_subcommand" -s x -l line-regexp -d 'Only show matches surrounded by line boundaries.'
complete -c rg -n "__fish_use_subcommand" -l mmap -d 'Search using memory maps when possible.'
complete -c rg -n "__fish_use_subcommand" -l no-mmap -d 'Never use memory maps.'
complete -c rg -n "__fish_use_subcommand" -s U -l multiline -d 'Enable matching across multiple lines.'
complete -c rg -n "__fish_use_subcommand" -l no-multiline
complete -c rg -n "__fish_use_subcommand" -l multiline-dotall -d 'Make \'.\' match new lines when multiline is enabled.'
complete -c rg -n "__fish_use_subcommand" -l no-multiline-dotall
complete -c rg -n "__fish_use_subcommand" -l no-config -d 'Never read configuration files.'
complete -c rg -n "__fish_use_subcommand" -l no-ignore -d 'Don\'t respect ignore files.'
complete -c rg -n "__fish_use_subcommand" -l ignore
complete -c rg -n "__fish_use_subcommand" -l no-ignore-global -d 'Don\'t respect global ignore files.'
complete -c rg -n "__fish_use_subcommand" -l ignore-global
complete -c rg -n "__fish_use_subcommand" -l no-ignore-messages -d 'Suppress gitignore parse error messages.'
complete -c rg -n "__fish_use_subcommand" -l ignore-messages
complete -c rg -n "__fish_use_subcommand" -l no-ignore-parent -d 'Don\'t respect ignore files in parent directories.'
complete -c rg -n "__fish_use_subcommand" -l ignore-parent
complete -c rg -n "__fish_use_subcommand" -l no-ignore-vcs -d 'Don\'t respect VCS ignore files.'
complete -c rg -n "__fish_use_subcommand" -l ignore-vcs
complete -c rg -n "__fish_use_subcommand" -l no-messages -d 'Suppress some error messages.'
complete -c rg -n "__fish_use_subcommand" -l messages
complete -c rg -n "__fish_use_subcommand" -l no-pcre2-unicode -d 'Disable Unicode mode for PCRE2 matching.'
complete -c rg -n "__fish_use_subcommand" -l pcre2-unicode
complete -c rg -n "__fish_use_subcommand" -s 0 -l null -d 'Print a NUL byte after file paths.'
complete -c rg -n "__fish_use_subcommand" -l null-data -d 'Use NUL as a line terminator instead of \\n.'
complete -c rg -n "__fish_use_subcommand" -l one-file-system -d 'Do not descend into directories on other file systems.'
complete -c rg -n "__fish_use_subcommand" -l no-one-file-system
complete -c rg -n "__fish_use_subcommand" -s o -l only-matching -d 'Print only matches parts of a line.'
complete -c rg -n "__fish_use_subcommand" -l passthru -d 'Print both matching and non-matching lines.'
complete -c rg -n "__fish_use_subcommand" -s P -l pcre2 -d 'Enable PCRE2 matching.'
complete -c rg -n "__fish_use_subcommand" -l no-pcre2
complete -c rg -n "__fish_use_subcommand" -l no-pre
complete -c rg -n "__fish_use_subcommand" -s p -l pretty -d 'Alias for --color always --heading --line-number.'
complete -c rg -n "__fish_use_subcommand" -s q -l quiet -d 'Do not print anything to stdout.'
complete -c rg -n "__fish_use_subcommand" -s z -l search-zip -d 'Search in compressed files.'
complete -c rg -n "__fish_use_subcommand" -l no-search-zip
complete -c rg -n "__fish_use_subcommand" -s S -l smart-case -d 'Smart case search.'
complete -c rg -n "__fish_use_subcommand" -l sort-files -d 'DEPRECATED'
complete -c rg -n "__fish_use_subcommand" -l no-sort-files
complete -c rg -n "__fish_use_subcommand" -l stats -d 'Print statistics about this ripgrep search.'
complete -c rg -n "__fish_use_subcommand" -l no-stats
complete -c rg -n "__fish_use_subcommand" -s a -l text -d 'Search binary files as if they were text.'
complete -c rg -n "__fish_use_subcommand" -l no-text
complete -c rg -n "__fish_use_subcommand" -l trim -d 'Trim prefixed whitespace from matches.'
complete -c rg -n "__fish_use_subcommand" -l no-trim
complete -c rg -n "__fish_use_subcommand" -l type-list -d 'Show all supported file types.'
complete -c rg -n "__fish_use_subcommand" -s u -l unrestricted -d 'Reduce the level of "smart" searching.'
complete -c rg -n "__fish_use_subcommand" -l vimgrep -d 'Show results in vim compatible format.'
complete -c rg -n "__fish_use_subcommand" -s H -l with-filename -d 'Print the file path with the matched lines.'
complete -c rg -n "__fish_use_subcommand" -l no-filename -d 'Never print the file path with the matched lines.'
complete -c rg -n "__fish_use_subcommand" -s w -l word-regexp -d 'Only show matches surrounded by word boundaries.'
complete -c rg -n "__fish_use_subcommand" -s h -l help -d 'Prints help information. Use --help for more details.'
complete -c rg -n "__fish_use_subcommand" -s V -l version -d 'Prints version information'
