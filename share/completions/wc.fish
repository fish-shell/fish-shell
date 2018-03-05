
# magic completion safety check (do not remove this comment)
if not type -q wc
    exit
end
complete -c wc -s c -l bytes -d "Print byte counts"
complete -c wc -s m -l chars -d "Print character counts"
complete -c wc -s l -l lines -d "Print newline counts"
complete -c wc -s L -l max-line-length -d "Print length of longest line"
complete -c wc -s w -l words -d "Print word counts"
complete -c wc -l help -d "Display help and exit"
complete -c wc -l version -d "Display version and exit"
