# Tab completion for rustc (https://github.com/rust-lang/rust).
# vim: tabstop=8 expandtab shiftwidth=4 softtabstop=4
complete -e -c rustc

complete -c rustc -s h -l help 

complete -c rustc -x -l cfg
complete -c rustc -r -s L -a 'dylib= static= framework='
complete -c rustc -x -s l -a 'dylib= static= framework='
complete -c rustc -x -l crate-type -a 'bin lib rlib dylib staticlib'
complete -c rustc -r -l crate-name
complete -c rustc -x -l emit -a 'asm llvm-bc llvm-ir obj link dep-info'
complete -c rustc -x -l print -a 'crate-name file-names sysroot'
complete -c rustc -s g
complete -c rustc -s O
complete -c rustc -r -l out-dir
complete -c rustc -x -l explain
complete -c rustc -l test
complete -c rustc -l target
complete -c rustc -x -l cap-lints
complete -c rustc -s V -l version -d 'Print version info and exit'
complete -c rustc -s v -l verbose -d 'Use verbose output'
complete -c rustc -f -l extern
complete -c rustc -f -l sysroot
complete -c rustc -x -l color -a 'auto always never'

set -l rust_docs (rustc -C help \
    | string replace -r -i '(\s+)-C(.+)(\s+)--(\s+)([^\n]+)' '$2 $5' \
    | string trim \
    | string match -r '^.*[^:]$')

for line in $rust_docs
    set docs (string split -m 1 ' ' $line)
    set flag (string replace -r '^([a-z\-]+\=|[a-z\-]+)(.*)' '$1' \
                                $docs[1])
    complete -c rustc -x -s C -l codegen -a "$flag" -d "$docs[2]"
end

set -l rust_docs (rustc -Z help \
    | string replace -r -i '(\s+)-Z(.+)--(\s+)([^\n]+)' '$2 $4' \
    | string trim \
    | string match -r '^.*[^:]$')

for line in $rust_docs
    set docs (string split -m 1 ' ' $line)
    set flag (string replace -r '^([a-z\-]+\=|[a-z\-]+)(.*)' '$1' \
                                   $docs[1])
    complete -c rustc -x -s Z -a "$flag" -d "$docs[2]"
end

set -l rust_docs (rustc -W help  \
    | egrep \
        '(\s+)(.+)(\s+)(allow|warn|deny|forbid)(\s+){2}([^\n]+)' \
    | string replace -r -i \
        '(\s+)(.+)(\s+)(allow|warn|deny|forbid)(\s+){2}([^\n]+)' '$2 $6' \
    | string match -r '^.*[^:]$' \
    | egrep -v '^(allow|warn|deny|forbid)$' \
    | egrep -v '^([a-z\-]+)(\s+)(allow|warn|deny|forbid)')

for line in $rust_docs
    set docs (string split -m 1 ' ' $line)
    complete -c rustc -x -s W -l warn -a "$docs[1]" -d "$docs[2]"
    complete -c rustc -x -s A -l allow -a "$docs[1]" -d "$docs[2]"
    complete -c rustc -x -s D -l deny -a "$docs[1]" -d "$docs[2]"
    complete -c rustc -x -s F -l forbid -a "$docs[1]" -d "$docs[2]"
end

set -e rust_codegen

