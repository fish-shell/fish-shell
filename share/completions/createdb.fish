complete -c createdb --no-files -a '(__fish_complete_pg_database)'

# Options:
complete -c createdb -s D -l tablespace -x -d "Default tablespace for the database"
complete -c createdb -s e -l echo -d "Show the commands being sent to the server"
complete -c createdb -s E -l encoding -x -d "Encoding for the database"
complete -c createdb -s l -l locale -x -d "Locale settings for the database"
complete -c createdb -l lc-collate -x -d "LC_COLLATE setting for the database"
complete -c createdb -l lc-ctype -x -d "LC_CTYPE setting for the database"
complete -c createdb -s O -l owner -x -a '(__fish_complete_pg_user)' -d "Database user to own the new database"
complete -c createdb -s T -l template -x -a '(__fish_complete_pg_database)' -d "Template database to copy"
complete -c createdb -s V -l version -d "Output version information, then exit"
complete -c createdb -s '?' -l help -d "Show help, then exit"

# Connection options:
complete -c createdb -s h -l host -x -a '(__fish_print_hostnames)' -d "Database server host or socket directory"
complete -c createdb -s p -l port -x -d "Database server port"
complete -c createdb -s U -l username -x -a '(__fish_complete_pg_user)' -d "User name to connect as"
complete -c createdb -s w -l no-password -d "Never prompt for password"
complete -c createdb -s W -l password -d "Force password prompt"
complete -c createdb -l maintenance-db -x -a '(__fish_complete_pg_database)' -d "Alternate maintenance database"
