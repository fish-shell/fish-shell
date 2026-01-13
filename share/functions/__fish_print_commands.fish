# localization: skip(private)
function __fish_print_commands --description "Print a list of documented fish commands"
    __fish_man1_pages |
        string match -rv '^fish-(?:changelog|completions|doc|tutorial|faq|for-bash-users|interactive|language|releasenotes|terminal-compatibility)$'
end
