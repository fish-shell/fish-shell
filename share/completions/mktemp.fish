if mktemp --version >/dev/null 2>/dev/null # GNU
    complete -c mktemp -s d -l directory -d 'create a directory, not a file'
    complete -c mktemp -s u -l dry-run -d 'do not create anything; merely print a name (unsafe)'
    complete -c mktemp -s q -l quiet -d 'suppress diagnostics about file/dir-creation failure'
    complete -c mktemp -l suffix -r -d 'append SUFF to TEMPLATE'
    complete -c mktemp -l tmpdir -d 'interpret TEMPLATE relative to DIR'
    complete -c mktemp -l help -d 'display this help and exit'
    complete -c mktemp -l version -d 'output version information and exit'
else # OS X
    complete -c mktemp -s d -d 'create a directory, not a file'
    complete -c mktemp -s q -d 'suppress diagnostics about file/dir-creation failure'
    complete -c mktemp -s t -r -d 'generate a template using PREFIX and TMPDIR (if set)'
    complete -c mktemp -s u -d 'file will be unlinked before mktemp exits (unsafe)'
end
