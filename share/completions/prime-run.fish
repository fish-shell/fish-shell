function __fish_complete_prime_run_subcommand
    set -lx -a PATH /usr/local/sbin /sbin /usr/sbin
    __fish_complete_subcommand
end

complete -c prime-run -x -a "(__fish_complete_prime_run_subcommand)"
