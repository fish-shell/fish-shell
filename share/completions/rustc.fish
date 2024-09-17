complete -c rustc -s h -l help

complete -c rustc -x -l cfg
complete -c rustc -x -l check-cfg -d "Provide list of expected cfgs for checking"
complete -c rustc -r -s L -a 'dependency= crate= native= framework= all='
complete -c rustc -x -s l -a 'dylib= static= framework='
complete -c rustc -x -l crate-type -a 'bin lib rlib dylib cdylib staticlib proc-macro'
complete -c rustc -r -l crate-name
complete -c rustc -x -l edition -a '2015 2018 2021' -d "Specify compiler edition"
complete -c rustc -x -l emit -a 'asm llvm-bc llvm-ir obj link dep-info metadata mir'
complete -c rustc -x -l print -d "Print compiler information" -xa "(rustc --print fake 2>&1 | string match -arg '`(.*?)`' | string match -v fake)"
complete -c rustc -s g -d "Equivalent to -C debuginfo=2"
complete -c rustc -s O -d "Equivalent to -C opt-level=2"
complete -c rustc -r -s o -d "Write output to <filename>"
complete -c rustc -r -l out-dir
complete -c rustc -x -l explain
complete -c rustc -l test
complete -c rustc -l target -xa "(rustc --print target-list)"
complete -c rustc -x -l cap-lints
complete -c rustc -s V -l version -d 'Print version info and exit'
complete -c rustc -s v -l verbose -d 'Use verbose output'
complete -c rustc -f -l extern
complete -c rustc -f -l sysroot
complete -c rustc -x -l color -a 'auto always never'

complete -c rustc -s C -l codegen -xa "(rustc -C help 2>/dev/null | string match -rv 'option is deprecated' | string replace -rf -i '(\s+)-C\s*(.+)=val(\s+)--(\s+)([^\n]+)' '\$2='\t'\$5')"

# rustc -Z is only available with the nightly toolchain, which may not be installed
function __fish_rustc_z_completions
    # This function takes a little over one second to run, so let's cache it. (It's ok if the first run is slow
    # since this is only run when specifically completing `rustc -Z<TAB>` and not when the completion is loaded.)
    if set -q __fish_cached_rustc_z_completions
        printf '%s\n' $__fish_cached_rustc_z_completions
        return
    end

    set -l rust_docs (rustc +nightly -Z help 2>/dev/null |
        string replace -r '^\s+' '' | string replace -ar ' +' ' ' | string replace -r '=val +-- +' '=\t')

    set -f flag
    set -f docs
    set -g __fish_cached_rustc_z_completions (for line in $rust_docs
        # Handle multi-line completions for options with values, e.g.
        #    -Z                                      unpretty=val -- present the input source, unstable (and less-pretty) variants;
        # `normal`, `identified`,
        # `expanded`, `expanded,identified`,
        # `expanded,hygiene` (with internal representations)
        if string match -rq '^-Z' -- $line
            # Actual -Z completion
            set -l parts (string split -- \t $line)
            set flag (string replace -r -- '-Z\s*' '' $parts[1])
            set docs $parts[2]
            printf '%s\t%s\n' $flag $docs
        else
            # Check for possible argument followed by description, supporting multiple completions per line
            # Don't split on ',' but rather ', ' to preserve commas in backticks
            # echo $flag:
            for line in (string split ', ' -- $line | string trim -c ',' | string trim)
                # echo \* $line
                if set -l parts (string match -gr '^(?:or )?`=?(.*?)`\s*(?:\((.*?)\)|: (.*))?\s*' -- $line)
                    set -l docs $parts[2] $parts[3]
                    printf '%s%s\t%s\n' $flag $parts[1] $docs
                else if set -l parts (string match -gr '^([a-z-]+):?\s*(.*)' -- $line)
                    printf '%s%s\t%s\n' $flag $parts[1] $parts[2]
                end | string match -ev '=val'
            end
        end
    end)
    __fish_rustc_z_completions
end
complete -c rustc -x -s Z -ka "(__fish_rustc_z_completions)"

function __fish_rustc_lint_completions
    rustc -W help 2>/dev/null \
        | string match -r \
        '(?:\s+)(?:.+)(?:\s+)(?:allow|warn|deny|forbid)(?:\s+){2}(?:[^\n]+)' \
        | string replace -r -i \
        '(\s+)(.+)(\s+)(allow|warn|deny|forbid)(\s+){2}([^\n]+)' '$2 $6' \
        | string match -r '^.*[^:]$' \
        | string match -r -v '^(allow|warn|deny|forbid)$' \
        | string match -r -v '^([a-z\-]+)(\s+)(allow|warn|deny|forbid)' \
        | string replace ' ' ,\t
end

# Support multiple arguments in one parameter as comma-separated values with `__fish_concat_completions`,
# combined with printing the name with a trailing , above.
complete -c rustc -s W -l warn -xa "(__fish_rustc_lint_completions | __fish_concat_completions)" -d "Warn on lint"
complete -c rustc -s A -l allow -xa "(__fish_rustc_lint_completions | __fish_concat_completions)" -d "Allow lint"
complete -c rustc -s D -l deny -xa "(__fish_rustc_lint_completions | __fish_concat_completions)" -d "Deny lint"
complete -c rustc -s F -l forbid -xa "(__fish_rustc_lint_completions | __fish_concat_completions)" -d "Forbid lint"
