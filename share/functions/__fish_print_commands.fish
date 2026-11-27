# localization: skip(private)
function __fish_print_commands --description "Print a list of documented fish commands"
    status get-file help_sections | string replace -rf '^cmds/' ''
end
