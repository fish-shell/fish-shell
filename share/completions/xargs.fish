#
#Completions for xargs
#

complete -c xargs -s 0 -l null -d "Input filenames are terminated by a null character instead of by whitespace, and the quotes and backslash are not special"
complete -c xargs -s e -l eof -d "Set the end of file string to eof-str"
complete -c xargs -s E -r -f -d "Set the end of file string to eof-str"
complete -c xargs -l help -d "Display help and exit"
complete -c xargs -s i -l replace -d "Replace replace-str in the initial arguments with names from standard input"
complete -c xargs -s I -r -f -d "Replace replace-str in the initial arguments with names from standard input"
complete -c xargs -s l -l max-lines -d "Use at most max-lines nonblank input lines per command line"
complete -c xargs -s L -r -f -d "Use at most max-lines nonblank input lines per command line"
complete -c xargs -s n -l max-args -r -f -d "Use at most max-args arguments per command line"
complete -c xargs -s p -l interactive -d "Prompt the user about whether to run each command line and read a line from the terminal"
complete -c xargs -s r -l no-run-if-empty -d "If the standard input does not contain any nonblanks, do not run the command"
complete -c xargs -s s -l max-chars -r -f -d "Use at most max-chars characters per command line"
complete -c xargs -s t -l verbose -d "Print the command line on the standard error output before executing it"
complete -c xargs -l version -d "Display version and exit"
complete -c xargs -s x -l exit -d "Exit if the size is exceeded"
complete -c xargs -s P -l max-procs -r -f -d "Run up to max-procs processes at a time"
