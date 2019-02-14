function __fish_print_cmd_args_without_options
    __fish_print_cmd_args | string match -re '^[^-]'
end
