if command base64 --ignore-garbage </dev/null >/dev/null 2>/dev/null
    complete -c base64 -l decode -s d -d "Decode data"
    complete -c base64 -l ignore-garbage -s i -d "When decoding, ignore non-alphabet characters"
    complete -c base64 -l wrap -s w -x -d "Wrap output after N characters (default 76)"
    complete -c base64 -l help -d "Display help"
    complete -c base64 -l version -d "Display version"
else
    complete -c base64 -s b -l break -x -d "Wrap output after N characters (default 76)"
    complete -c base64 -s d -s D -l decode -d "Decode data"
    complete -c base64 -s i -l input -r -d "Read from file"
    complete -c base64 -s o -l output -r -d "Write to file"
    complete -c base64 -s h -l help -d "Display help"
end
