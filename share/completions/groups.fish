complete -c groups -x -a "(__fish_complete_users)"
if __fish_supports_version groups
    complete -c groups -l help -d 'Display help message'
    complete -c groups -l version -d 'Display version information'
end
