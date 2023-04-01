function __ghoti_complete_prime_run_subcommand
    set -lx -a PATH /usr/local/sbin /sbin /usr/sbin
    __ghoti_complete_subcommand
end

complete -c prime-run -x -a "(__ghoti_complete_prime_run_subcommand)"
