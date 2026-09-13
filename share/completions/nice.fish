complete -c nice -a "(__fish_complete_subcommand -- -n --adjustment)" -d Command

set -l is_gnu
__fish_supports_version nice; and set is_gnu --is-gnu

__fish_gnu_complete -c nice -s n -l adjustment -n __fish_no_arguments -d "Add specified amount to niceness value" -x $is_gnu

if test -n "$is_gnu"
    complete -c nice -l help -n __fish_no_arguments -d "Display help and exit"
    complete -c nice -l version -n __fish_no_arguments -d "Display version and exit"
end
