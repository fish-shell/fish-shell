function __fish_is_git_repository --description 'Check if the current directory is a git repository'
    git rev-parse --is-inside-work-tree 2>/dev/null >/dev/null
end
