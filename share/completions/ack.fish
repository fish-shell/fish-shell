# Searching
complete -c ack -s i -l ignore-case -d 'Ignore case'
complete -c ack -l smart-case -d 'Ignore case when pattern contains no uppercase'
complete -c ack -l nosmart-case -l no-smart-case -d 'Don\'t ignore case'
complete -c ack -s v -l invert-match -d 'Invert match'
complete -c ack -s w -l word-regexp -d 'Match only whole words'
complete -c ack -s Q -l literal -d 'Quote all metacharacters'

# Search output
complete -c ack -l lines -d 'Only print line(s) NUM of each file'
complete -c ack -s l -l files-with-matches -d 'Only print filenames containing matches'
complete -c ack -s L -l files-without-matches -d 'Only print filenames with no matches'
complete -c ack -l output -d 'Output the evaluation of Perl expression for each line'
complete -c ack -s o -d 'Output the part of line matching pattern'
complete -c ack -l passthru -d 'Print all lines'
complete -c ack -l match -d 'Specify pattern explicitly'
complete -c ack -s m -l max-count -d 'Stop searching in each file after NUM matches'
complete -c ack -s 1 -d 'Stop searching after first match'
complete -c ack -s H -l with-filename -d 'Print the filename for each match'
complete -c ack -s h -l no-filename -d 'Suppress the prefixing filename on output'
complete -c ack -s c -l count -d 'Show number of lines matching per file'
complete -c ack -l column -d 'Show column number of first match'
complete -c ack -l nocolumn -l no-column -d 'Don\'t show column number of first match'
complete -c ack -x -s A -l after-context -d 'Print NUM lines of trailing context'
complete -c ack -x -s B -l before-context -d 'Print NUM lines of leading context'
complete -c ack -x -s C -l context -d 'Print NUM lines of context'
complete -c ack -l print0 -d 'Print null byte as separator between filenames'
complete -c ack -s s -d 'Suppress error messages about file errors'

# File presentation
complete -c ack -l pager -d 'Pipes all ack output through command'
complete -c ack -l nopager -l no-pager -d 'Do not send output through a pager'
complete -c ack -l heading -d 'Prints a filename heading above file\'s results'
complete -c ack -l noheading -l no-heading -d 'Don\'t print a filename heading above file\'s results'
complete -c ack -l break -d 'Print a break between results'
complete -c ack -l nobreak -l no-break -d 'Don\'t print a break between results'
complete -c ack -l group -d 'Filename heading and line break between results'
complete -c ack -l nogroup -l no-group -d 'No filename heading and no line breaks between results'
complete -c ack -l color -d 'Highlight the matching text'
complete -c ack -l nocolor -l no-color -l nocolour -l no-colour -d 'Don\'t highlight the matching text'
complete -c ack -l color-filename -d 'Set the color for filenames'
complete -c ack -l color-match -d 'Set the color for matches'
complete -c ack -l color-lineno -d 'Set the color for line numbers'
complete -c ack -l flush -d 'Flush output immediately'

# File finding
complete -c ack -s f -d 'Only print the files selected'
complete -c ack -s g -d 'Only select files matching pattern'
complete -c ack -l sort-files -d 'Sort the found files lexically'
complete -c ack -l show-types -d 'Show which types each file has'
complete -c ack -l files-from -d 'Read the list of files to search from file'
complete -c ack -s x -d 'Read the list of files to search from STDIN'

# File inclusion/exclusion
complete -c ack -l ignore-dir -l ignore-directory -d 'Ignore directory'
complete -c ack -l noignore-dir -l no-ignore-dir -l noignore-directory -l no-ignore-directory -d 'Don\'t ignore directory'
complete -c ack -l ignore-file -d 'Add filter for ignoring files'
complete -c ack -s r -s R -l recurse -d 'Recurse into subdirectories'
complete -c ack -s n -l no-recurse -d 'No descending into subdirectories'
complete -c ack -l follow -d 'Follow symlinks'
complete -c ack -l nofollow -l no-follow -d 'Don\'t follow symlinks'
complete -c ack -s k -l known-types -d 'Include only recognized files'
complete -c ack -l type -d 'Include only X files'

# File type specification
complete -c ack -l type-set -d 'Replaces definition of type'
complete -c ack -l type-add -d 'Specify definition of type'
complete -c ack -l type-del -d 'Removes all filters associated with type'

# Miscellaneous
complete -c ack -l noenv -l no-env -d 'Ignores environment variables and ackrc files'
complete -c ack -l ackrc -d 'Specifies location of ackrc file'
complete -c ack -l ignore-ack-defaults -d 'Ignore default definitions ack includes'
complete -c ack -l create-ackrc -d 'Outputs default ackrc'
complete -c ack -s \? -l help -d 'Shows help'
complete -c ack -l help-types -d 'Shows all known types'
complete -c ack -l dump -d 'Dump information on which options are loaded'
complete -c ack -l filter -d 'Forces ack to treat input as a pipe'
complete -c ack -l nofilter -l no-filter -d 'Forces ack to treat input as tty'
complete -c ack -l man -d 'Shows man page'
complete -c ack -l version -d 'Displays version and copyright'
complete -c ack -l thpppt -d 'Bill the Cat'
complete -c ack -l bar -d 'The warning admiral'

# File types
if type ack >/dev/null
    for type in (ack --dump 2>/dev/null | perl -lne 'print $1 if /^\s+--type-add=([^:]+)/' | uniq)
        complete -c ack -l $type -d "Allow $type file type"
        complete -c ack -l no$type -l no-$type -d "Don't allow $type file type"
    end
end
