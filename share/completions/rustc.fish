complete -c rustc -s h -l help

complete -c rustc -x -l cfg
complete -c rustc -r -s L -a 'dependency= crate= native= framework= all='
complete -c rustc -x -s l -a 'dylib= static= framework='
complete -c rustc -x -l crate-type -a 'bin lib rlib dylib staticlib proc-macro'
complete -c rustc -r -l crate-name
complete -c rustc -x -l emit -a 'asm llvm-bc llvm-ir obj link dep-info metadata mir'
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
    set -l docs (string split -m 1 ' ' $line)
    set -l flag (string replace -r '^([a-z\-]+\=|[a-z\-]+)(.*)' '$1' \
                                $docs[1])
    complete -c rustc -x -s C -l codegen -a (string escape -- "$flag") -d "$docs[2]"
end

# rustc -Z is only available with the nightly toolchain, which may not be installed
if rustc +nightly >/dev/null 2>&1
    set -l rust_docs (rustc +nightly -Z help \
        | string replace -r -i '(\s+)-Z(.+)--(\s+)([^\n]+)' '$2 $4' \
        | string trim \
        | string match -r '^.*[^:]$')

    for line in $rust_docs
        set -l docs (string split -m 1 ' ' $line)
        set -l flag (string replace -r '^([a-z\-]+\=|[a-z\-]+)(.*)' '$1' \
                                       $docs[1])
        complete -c rustc -x -s Z -a (string escape -- "$flag") -d "$docs[2]"
    end
end

set -l rust_docs (rustc -W help  \
    | string match -r \
        '(?:\s+)(?:.+)(?:\s+)(?:allow|warn|deny|forbid)(?:\s+){2}(?:[^\n]+)' \
    | string replace -r -i \
        '(\s+)(.+)(\s+)(allow|warn|deny|forbid)(\s+){2}([^\n]+)' '$2 $6' \
    | string match -r '^.*[^:]$' \
    | string match -r -v '^(allow|warn|deny|forbid)$' \
    | string match -r -v '^([a-z\-]+)(\s+)(allow|warn|deny|forbid)')

for line in $rust_docs
    set -l docs (string split -m 1 ' ' $line)
    complete -c rustc -x -s W -l warn -a (string escape -- "$docs[1]") -d "$docs[2]"
    complete -c rustc -x -s A -l allow -a (string escape -- "$docs[1]") -d "$docs[2]"
    complete -c rustc -x -s D -l deny -a (string escape -- "$docs[1]") -d "$docs[2]"
    complete -c rustc -x -s F -l forbid -a (string escape -- "$docs[1]") -d "$docs[2]"
end
