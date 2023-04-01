function __ghoti_mysql_query -a query
    argparse -i 'u/user=' 'P/port=' 'h/host=' 'p/password=?' 'S/socket=' -- (commandline -po)
    set -l mysql_cmd mysql
    for flag in u P h S
        if set -q _flag_$flag
            set -l flagvar _flag_$flag
            set -a mysql_cmd -$flag $$flagvar
        end
    end
    if test -n "$_flag_p"
        set -a mysql_cmd -p$_flag_p
    end
    echo $query | $mysql_cmd 2>/dev/null
end

function __ghoti_complete_mysql_databases
    __ghoti_mysql_query 'show databases'
end

function __ghoti_complete_mysql
    set -l command $argv[1]

    complete -c $command -s D -l database -x -d 'The database to use' -a '(__ghoti_complete_mysql_databases)'
    complete -c $command -a '(__ghoti_complete_mysql_databases)'
end
