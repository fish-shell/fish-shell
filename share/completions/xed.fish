switch (uname -s)
    case Darwin
        complete -c xed -s x -d 'Launch Xcode if needed'
        complete -c xed -s c -d 'Create a new document'
        complete -c xed -s w -d 'Wait for the document to close before exiting'
        complete -c xed -s r -d 'Open file(s) read-only'
        complete -c xed -s b -d 'Open Xcode without activating it'
        complete -c xed -s h -d 'Show help and exit'
        complete -c xed -s v -d 'Print version and exit'
        complete -c xed -s l -x -d 'Open at the given line number'
    case '*'
        complete -c xed -s '?' -s h -l help -d 'Show help and exit'
        complete -c xed -l version -d 'Show version and exit'
        complete -c xed -l help-all -d 'Show all options and exit'

        complete -c xed -l display -d 'X display'
        complete -c xed -l encoding -d Encoding
        complete -c xed -l new-window -d 'Create a new window'
        complete -c xed -l new-document -d 'Create a new document'
        complete -c xed -l list-encodings -d 'List encodings'
end
