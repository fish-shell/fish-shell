function __fish_print_connman_services
    connmanctl services | string replace -r '.* (.*)' '$1'
end
