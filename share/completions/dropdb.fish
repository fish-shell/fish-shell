complete -c dropdb --no-files -a '(__fish_complete_pg_database)'

# Options:
complete -c dropdb -s e -l echo -d "show the commands being sent to the server"
complete -c dropdb -s i -l interactive -d "prompt before deleting anything"
complete -c dropdb -s V -l version -d "output version information, then exit"
complete -c dropdb -l if-exists -d "don't report error if database doesn't exist"
complete -c dropdb -s '?' -l help -d "show help, then exit"

# Connection options:
complete -c dropdb -s h -l host -x -a '(__fish_print_hostnames)' -d "database server host or socket directory"
complete -c dropdb -s p -l port -x -d "database server port"
complete -c dropdb -s U -l username -x -a '(__fish_complete_pg_user)' -d "user name to connect as"
complete -c dropdb -s w -l no-password -d "never prompt for password"
complete -c dropdb -s W -l password -d "force password prompt"
complete -c dropdb -l maintenance-db -x -a '(__fish_complete_pg_database)' -d "alternate maintenance database"
