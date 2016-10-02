function __fish_print_connman_vpnconnections
    connmanctl vpnconnections | string replace -r '.* (.*)' '$1'
end
