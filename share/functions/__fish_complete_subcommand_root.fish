function __fish_complete_subcommand_root -d "Run the __fish_complete_subcommand function using a PATH containing /sbin and /usr/sbin"
    set -lx -p PATH /sbin /usr/sbin 2>/dev/null
    __fish_complete_subcommand $argv
end
