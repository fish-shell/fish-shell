# Completions for make
function __fish_print_make_targets --argument-names directory file
    # Since we filter based on localized text, we need to ensure the
    # text will be using the correct locale.
    set -lx LC_ALL C

    set -l makeflags -C $directory
    if test -n "$file"
        set -a makeflags -f $file
    end

    if make --version 2>/dev/null | string match -q 'GNU*'
        # https://stackoverflow.com/a/26339924
        make $makeflags -pRrq : 2>/dev/null |
            awk -F: -v 'bs_regex=\\\\\\\\' '/^# Files/,/^# Finished Make data base/ {
                if ($1 == "# Not a target") skip = 1;
                if ($1 !~ "^[#.\t]" && !is_continuation ) {
                    if (!skip) print $1;
                    skip = 0
                }
                is_continuation = $0 ~ "^([^#]*[^#" bs_regex "])?(" bs_regex bs_regex ")*" bs_regex "$";
            }' 2>/dev/null
    else
        # BSD make
        make $makeflags -d g1 -rn >/dev/null 2>| awk -F, '/^#\*\*\* Input graph:/,/^$/ {if ($1 !~ "^#... ") {gsub(/# /,"",$1); print $1}}' 2>/dev/null
    end
end

function __fish_complete_make_targets
    set -l directory (string replace -rf '^make .*(-C ?|--directory(=| +))([^ ]*) .*$' '$3' -- $argv)
    or set directory .
    set -l file (string replace -rf '^make .*(-f ?|--file(=| +))([^ ]*) .*$' '$3' -- $argv)
    __fish_print_make_targets $directory $file
end

# This completion reenables file completion on
# assignments, so e.g. 'make foo FILES=<tab>' will receive standard
# filename completion.
complete -c make -n 'commandline -ct | string match -q "*=*"' -a "(__fish_complete_make_targets (commandline -p))" -d Target
complete -f -c make -n 'commandline -ct | not string match -q "*=*"' -a "(__fish_complete_make_targets (commandline -p))" -d Target
complete -c make -s f -d "Use file as makefile" -r
complete -x -c make -s C -l directory -x -a "(__fish_complete_directories (commandline -ct))" -d "Change directory"
complete -c make -s d -d "Debug mode"
complete -c make -s e -d "Environment before makefile"
complete -c make -s i -d "Ignore errors"
complete -x -c make -s I -d "Search directory for makefile" -a "(__fish_complete_directories (commandline -ct))"
complete -f -c make -s j -d "Number of concurrent jobs (no argument means no limit)"
complete -c make -s k -d "Continue after an error"
complete -c make -s l -d "Start when load drops"
complete -c make -s n -d "Do not execute commands"
complete -c make -s o -r -d "Ignore specified file"
complete -c make -s p -d "Print database"
complete -c make -s q -d "Question mode"
complete -c make -s r -d "Eliminate implicit rules"
complete -c make -s s -d "Quiet mode"
complete -c make -s S -d "Don't continue after an error"
complete -c make -s t -d "Touch files, don't run commands"
complete -c make -s v -d "Display version and exit"
complete -c make -s w -d "Print working directory"
complete -c make -s W -r -d "Pretend file is modified"
