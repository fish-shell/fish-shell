
# magic completion safety check (do not remove this comment)
if not type -q gofmt
    exit
end
complete -c gofmt -l cpuprofile -o cpuprofile -d "Write cpu profile to this file"
complete -c gofmt -s d -d "Display diffs instead of rewriting files"
complete -c gofmt -s e -d "Report all errors (not just the first 10 on different lines)"
complete -c gofmt -s l -d "List files whose formatting differs from gofmt's"
complete -c gofmt -s r -d "Rewrite rule (e.g., 'a[b:len(a)] -> a[b:]')"
complete -c gofmt -s s -d "Simplify code"
complete -c gofmt -s w -d "Write result to (source) file instead of stdout"
complete -c gofmt -l help -o help -s h -d "Show help"
