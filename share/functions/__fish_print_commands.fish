# localization: skip(private)
function __fish_print_commands --description "Print a list of documented fish commands"
    if set -q __fish_data_dir[1] && test -d $__fish_data_dir/man/man1/
        printf %s\n $__fish_data_dir/man/man1/**.1*
    else
        status list-files man/man1/ 2>/dev/null
    end |
        string replace -r '.*/' '' |
        string replace -r '.1(.gz)?$' '' |
        string match -rv '^fish-(?:changelog|completions|doc|tutorial|faq|for-bash-users|interactive|language|releasenotes|terminal-compatibility)$'
end
