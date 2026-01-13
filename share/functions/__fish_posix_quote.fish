# localization: skip(private)
function __fish_posix_quote
    for arg in $argv
        if set -l bareword (string match -r -- '^[-=\w]+$' $arg)
            printf "%s " $bareword
            continue
        end
        printf "'%s' " (printf %s $arg | string replace -a -- "'" "'\\''" |
            string collect --no-trim-newlines)
    end
end
