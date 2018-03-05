
# magic completion safety check (do not remove this comment)
if not type -q goimports
    exit
end
complete -c goimports -s d -d "Display diffs instead of rewriting files"
complete -c goimports -s e -d "Report all errors"
complete -c goimports -s l -d "List files whose formatting differs from goimport's"
complete -c goimports -l srcdir -o srcdir -r -d "Choose imports as if source code is from dir"
complete -c goimports -s w -d "Write result to (source) file instead of stdout"
complete -c goimports -l help -o help -s h -d "Show help"
