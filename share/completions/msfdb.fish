complete -c msfdb -f

# Commands
complete -c msfdb -n __fish_use_subcommand -x -a init -d 'Initialize the component'
complete -c msfdb -n __fish_use_subcommand -x -a reinit -d 'Delete and reinitialize the component'
complete -c msfdb -n __fish_use_subcommand -x -a delete -d 'Delete and stop the component'
complete -c msfdb -n __fish_use_subcommand -x -a status -d 'Check component status'
complete -c msfdb -n __fish_use_subcommand -x -a start -d 'Start the component'
complete -c msfdb -n __fish_use_subcommand -x -a stop -d 'Stop the component'
complete -c msfdb -n __fish_use_subcommand -x -a restart -d 'Restart the component'

# General Options
complete -c msfdb -l component -x -a 'all database webservice' -d 'Component used with provided command'
complete -c msfdb -s d -l debug -d 'Enable debug output'
complete -c msfdb -s h -l help -d 'Show help message'
complete -c msfdb -l use-defaults -d 'Accept all defaults and do not prompt for options'

# Database Options
complete -c msfdb -l msf-db-name -x -d 'Database name'
complete -c msfdb -l msf-db-user-name -x -d 'Database username'
complete -c msfdb -l msf-test-db-name -x -d 'Test database name'
complete -c msfdb -l msf-test-db-user-name -x -d 'Test database username'
complete -c msfdb -l db-port -x -d 'Database port'
complete -c msfdb -l db-pool -x -d 'Database connection pool size'

# Web Service Options
complete -c msfdb -s a -l address -x -a "(__fish_print_addresses --all)" -d 'Bind to host address'
complete -c msfdb -s p -l port -x -d 'Web service port'
complete -c msfdb -l ssl -d 'Enable SSL'
complete -c msfdb -l no-ssl -d 'Disable SSL'
complete -c msfdb -l ssl-key-file -F -d 'Path to private key'
complete -c msfdb -l ssl-cert-file -F -d 'Path to certificate'
complete -c msfdb -l ssl-disable-verify -d 'Disables (optional) client cert requests'
complete -c msfdb -l no-ssl-disable-verify -d 'Enables (optional) client cert requests'
complete -c msfdb -l environment -xa 'production development' -d 'Web service framework environment'
complete -c msfdb -l retry-max -x -d 'Maximum number of web service connect attempts'
complete -c msfdb -l retry-delay -x -d 'Delay (seconds) between web service connect attempts'
complete -c msfdb -l user -x -d 'Initial web service admin username'
complete -c msfdb -l pass -x -d 'Initial web service admin password'
complete -c msfdb -l msf-data-service -x -d 'Local msfconsole data service connection name'
complete -c msfdb -l no-msf-data-service -x -d 'Disable local msfconsole data service connection'
