if command -q wc && command wc --version >/dev/null 2>/dev/null
    complete -c wc -s c -l bytes -d "Print byte count"
    complete -c wc -s m -l chars -d "Print character count"
    complete -c wc -s l -l lines -d "Print number of lines"
    complete -c wc -s L -l max-line-length -d "Print length of longest line"
    complete -c wc -s w -l words -d "Print number of words"
    complete -c wc -l files0-from -d "Read input from NUL-terminated filenames in the given file" -r
    complete -c wc -l help -d "Display help and exit"
    complete -c wc -l version -d "Display version and exit"
else
    complete -c wc -s c -d "Print byte count"
    complete -c wc -s m -d "Print character count"
    complete -c wc -s l -d "Print number of lines"
    complete -c wc -s w -d "Print number of words"
    switch (uname -s)
        case FreeBSD NetBSD
            complete -c wc -s L -d "Print length of longest line"
        case OpenBSD
            complete -c wc -s h -d "Use B, KB, MB, GB, TB, PB, EB unit suffixes"
    end
end
