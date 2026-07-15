function __fish_osascript_languages
    osalang
end

complete -c osascript -d 'Enter one line of script (may be repeated to build multi-line script)' -s e -r
complete -d 'Interactive mode: prompt for one line at a time, print result after each' -c osascript -s i
complete -c osascript -d 'Override language for plain-text files (default: AppleScript)' -s l -x -a '(__fish_osascript_languages)'
complete -c osascript -d 'Output style modifier(s): h/s = value format, e/o = error destination' -s s -x -a '
h\t"Human-readable output (default)"
s\t"Recompilable source form"
e\t"Print errors to stderr (default)"
o\t"Print errors to stdout"
he\t"Human-readable + errors to stderr"
ho\t"Human-readable + errors to stdout"
se\t"Source form + errors to stderr"
so\t"Source form + errors to stdout"'
complete -c osascript -f -a '(__fish_complete_suffix .scpt)' -d 'Compiled script'
complete -c osascript -f -a '(__fish_complete_suffix .applescript)' -d 'AppleScript source file'
complete -c osascript -d 'JavaScript for Automation source file' -f -a '(__fish_complete_suffix .js)'
