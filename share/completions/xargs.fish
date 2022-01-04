#
#Completions for xargs
#

complete -c xargs -n __fish_is_first_token -s 0 -l null -d "Terminate filenames with a \0 instead of whitespace, ignore quotes and backslash"
complete -c xargs -n __fish_is_first_token -s e -l eof -d "Set the end of file string to eof-str"
complete -c xargs -n __fish_is_first_token -s E -r -f -d "Set the end of file string to eof-str"
complete -c xargs -n __fish_is_first_token -l help -d "Display help and exit"
complete -c xargs -n __fish_is_first_token -s i -l replace -d "Replace replace-str in the initial arguments with names from standard input"
complete -c xargs -n __fish_is_first_token -s I -r -f -d "Replace replace-str in the initial arguments with names from standard input"
complete -c xargs -n __fish_is_first_token -s l -l max-lines -d "Use at most max-lines nonblank input lines per command line"
complete -c xargs -n __fish_is_first_token -s L -r -f -d "Use at most max-lines nonblank input lines per command line"
complete -c xargs -n __fish_is_first_token -s n -l max-args -r -f -d "Use at most max-args arguments per command line"
complete -c xargs -n __fish_is_first_token -s p -l interactive -d "Prompt the user before running each command line"
complete -c xargs -n __fish_is_first_token -s r -l no-run-if-empty -d "If the standard input does not contain any nonblanks, do not run the command"
complete -c xargs -n __fish_is_first_token -s s -l max-chars -r -f -d "Use at most max-chars characters per command line"
complete -c xargs -n __fish_is_first_token -s t -l verbose -d "Print the command line on the standard error output before executing it"
complete -c xargs -n __fish_is_first_token -l version -d "Display version and exit"
complete -c xargs -n __fish_is_first_token -s x -l exit -d "Exit if the size is exceeded"
complete -c xargs -n __fish_is_first_token -s P -l max-procs -r -f -d "Run up to max-procs processes at a time"
complete -c xargs -xa "(__fish_complete_subcommand)"
