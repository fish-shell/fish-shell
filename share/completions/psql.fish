
function __fish_complete_pg_database
    psql -AtqwlF \t ^/dev/null | awk 'NF > 1 { print $1 }'
end

function __fish_complete_pg_user
    psql -Atqwc 'select usename from pg_user' template1 ^/dev/null
end

complete -c psql --no-files -a '(__fish_complete_pg_database)'


#
# General options:
#

complete -c psql -s d -l dbname -a '(__fish_complete_pg_database)'      --description "database name to connect to"
complete -c psql -s c -l command        --description "run only single command (SQL or internal) and exit"
complete -c psql -s f -l file -r        --description "execute commands from file, then exit"
complete -c psql -s l -l list           --description "list available databases, then exit"

# complete -c psql -s v -l set=, --variable=NAME=VALUE
#                                           set psql variable NAME to VALUE

complete -c psql -s X -l no-psqlrc          --description "do not read startup file (~/.psqlrc)"
complete -c psql -s 1 -l single-transaction --description "execute command file as a single transaction"
complete -c psql -l help                    --description "show this help, then exit"
complete -c psql -l version                 --description "output version information, then exit"

#
#   Input and output options:
#

complete -c psql -s a, -l echo-all           --description "echo all input from script"
complete -c psql -s e, -l echo-queries       --description "echo commands sent to server"
complete -c psql -s E, -l echo-hidden        --description "display queries that internal commands generate"
complete -c psql -s L, -l log-file           --description "send session log to file"
complete -c psql -s n, -l no-readline        --description "disable enhanced command line editing (readline)"
complete -c psql -s o, -l output             --description "send query results to file (or |pipe)"
complete -c psql -s q, -l quiet              --description "run quietly (no messages, only query output)"
complete -c psql -s s, -l single-step        --description "single-step mode (confirm each query)"
complete -c psql -s S, -l single-line        --description "single-line mode (end of line terminates SQL command)"

#
#   Output format options:
#

complete -c psql -s A, -l no-align          --description "unaligned table output mode"
complete -c psql -s H, -l html              --description "HTML table output mode"
complete -c psql -s P, -l pset              --description "set printing option VAR to ARG (see \pset command)"
complete -c psql -s t, -l tuples-only       --description "print rows only"
complete -c psql -s T, -l table-attr        --description "set HTML table tag attributes (e.g., width, border)"
complete -c psql -s x, -l expanded          --description "turn on expanded table output"
complete -c psql -s F, -l field-separator   --description "set field separator (default: '|')"
complete -c psql -s R, -l record-separator  --description "set record separator (default: newline)"


#
# Connection options
#

complete -c psql -s h -l host -a '(__fish_print_hostnames)' --description "database server host or socket directory"
complete -c psql -s p -l port -x --description "database server port"
complete -c psql -s U -l username -a '(__fish_complete_pg_user)' --description "database user name"
complete -c psql -s w -l no-password --description "never prompt for password"
complete -c psql -s W -l password --description "force password prompt (should happen automatically)"
