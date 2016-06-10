complete -c gorename -s d -d "Display diffs instead of rewriting files"
complete -c gorename -l diffcmd -o diffcmd -d "Diff command invoked when using -d" -r
complete -c gorename -l force -o force -d "Proceed, even if conflicts were reported"
complete -c gorename -l from -o from -d "Identifier to be renamed" -x
complete -c gorename -l help -o help -d "Show usage message"
complete -c gorename -l offset -o offset -d "File and byte offset of identifier to be renamed" -x
complete -c gorename -l tags -o tags -d "Build tag"
complete -c gorename -l to -o to -d "New name for identifier" -r
complete -c gorename -s v -d "Print verbose information"
complete -c gorename -l help -o help -s h -d "Show help"
