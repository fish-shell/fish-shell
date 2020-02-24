complete -c dropdb --no-files -a '(__fish_complete_pg_database)'

# Options:
complete -c dropdb -s e -l echo -d "Show the commands being sent to the server"
complete -c dropdb -s i -l interactive -d "Prompt before deleting anything"
complete -c dropdb -s V -l version -d "Output version information, then exit"
complete -c dropdb -l if-exists -d "Don't report error if database doesn't exist"
complete -c dropdb -s '?' -l help -d "Show help, then exit"

# Connection options:
complete -c dropdb -s h -l host -x -a '(__fish_print_hostnames)' -d "Database server host or socket directory"
complete -c dropdb -s p -l port -x -d "Database server port"
complete -c dropdb -s U -l username -x -a '(__fish_complete_pg_user)' -d "User name to connect as"
complete -c dropdb -s w -l no-password -d "Never prompt for password"
complete -c dropdb -s W -l password -d "Force password prompt"
complete -c dropdb -l maintenance-db -x -a '(__fish_complete_pg_database)' -d "Alternate maintenance database"
