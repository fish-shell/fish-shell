# localization: tier1
function __fish_default_command_not_found_handler
    printf (_ "fish: Unknown command: %s\n") (string escape -- $argv[1]) >&2
end
