# localization: skip(private)
function __fish_print_commands --description "Print a list of documented fish commands"
    __fish_data_list_files man/man1 |
        string replace -r '.*/' '' |
        string replace -r '.1(.gz)?$' '' |
        string match -rv '^fish-(?:changelog|completions|doc|tutorial|faq|for-bash-users|interactive|language|releasenotes|terminal-compatibility)$'
end
