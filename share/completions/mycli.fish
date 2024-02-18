# fish completion for mycli                              -*- shell-script -*-

function __mycli_complete_dsn
    mycli --list-dsn
end

complete -c mycli -s d -l dsn -x -d 'Use DSN configured into the [alias_dsn]' -a '(__mycli_complete_dsn)'
complete -c mycli -a '(__mycli_complete_dsn)'