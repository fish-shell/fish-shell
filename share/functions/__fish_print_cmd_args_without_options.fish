function __ghoti_print_cmd_args_without_options
    __ghoti_print_cmd_args | string match -re '^[^-]'
end
