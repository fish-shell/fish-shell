function __fish_print_connman_technologies
    connmanctl technologies | string match '*Type*' | string replace -r '  Type = (.*)' '$1'
end
