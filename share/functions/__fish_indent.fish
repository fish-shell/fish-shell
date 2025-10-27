# TODO(embed-data) Remove this after that has been rolled out (on-by-default).

# localization: skip(private)
set -l dir "$(path dirname -- (status fish-path))"

if command -v $dir/fish_indent >/dev/null
    function __fish_indent --wraps fish_indent --inherit-variable dir
        $dir/fish_indent $argv
    end
else
    function __fish_indent --wraps fish_indent
        fish_indent $argv
    end
end
