set -l dir "$(path dirname -- (status fish-path 2>/dev/null))"

if command -v $dir/fish_indent >/dev/null
    function __fish_indent --wraps fish_indent --inherit-variable dir
        $dir/fish_indent $argv
    end
else
    function __fish_indent --wraps fish_indent
        fish_indent $argv
    end
end
