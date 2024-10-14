function __fish_indent
    set -l dir "$(path dirname -- (status fish-path 2>/dev/null))"
    if command -v $dir/fish_indent >/dev/null
        $dir/fish_indent $argv
    else
        fish_indent $argv
    end
end
