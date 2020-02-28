complete -c psql --no-files -a '(__fish_complete_pg_database)'

# General options:
complete -c psql -s d -l dbname -x -a '(__fish_complete_pg_database)' -d "Database name to connect to"
complete -c psql -s c -l command -d "Run only single command (SQL or internal) and exit"
complete -c psql -s f -l file -r -d "Execute commands from file, then exit"
complete -c psql -s l -l list -d "List available databases, then exit"
complete -c psql -s v -l set -l variable -x -d "Set psql variable NAME to VALUE"
complete -c psql -s X -l no-psqlrc -d "Do not read startup file (~/.psqlrc)"
complete -c psql -s 1 -l single-transaction -d "Execute command file as a single transaction"
complete -c psql -s '?' -l help -a "options commands variables" -d "Show this help, then exit"
complete -c psql -l version -d "Output version information, then exit"

# Input and output options:
complete -c psql -s a -l echo-all -d "Echo all input from script"
complete -c psql -s b -l echo-errors -d "Echo failed commands"
complete -c psql -s e -l echo-queries -d "Echo commands sent to server"
complete -c psql -s E -l echo-hidden -d "Display queries that internal commands generate"
complete -c psql -s L -l log-file -r -d "Send session log to file"
complete -c psql -s n -l no-readline -d "Disable enhanced command line editing (readline)"
complete -c psql -s o -l output -r -d "Send query results to file (or |pipe)"
complete -c psql -s q -l quiet -d "Run quietly (no messages, only query output)"
complete -c psql -s s -l single-step -d "Single-step mode (confirm each query)"
complete -c psql -s S -l single-line -d "Single-line mode (end of line terminates SQL command)"

# Output format options:
complete -c psql -s A -l no-align -d "Unaligned table output mode"
complete -c psql -s H -l html -d "HTML table output mode"
complete -c psql -s P -l pset -d "Set printing option VAR to ARG (see \pset command)"
complete -c psql -s t -l tuples-only -d "Print rows only"
complete -c psql -s T -l table-attr -d "Set HTML table tag attributes (e.g., width, border)"
complete -c psql -s x -l expanded -d "Turn on expanded table output"
complete -c psql -s F -l field-separator -x -d "Set field separator (default: '|')"
complete -c psql -s R -l record-separator -x -d "Set record separator (default: newline)"
complete -c psql -s 0 -l record-separator-zero -d "Set record separator for unaligned output to zero byte"

# Connection options:
complete -c psql -s h -l host -x -a '(__fish_print_hostnames)' -d "Database server host or socket directory"
complete -c psql -s p -l port -x -d "Database server port"
complete -c psql -s U -l username -x -a '(__fish_complete_pg_user)' -d "Database user name"
complete -c psql -s w -l no-password -d "Never prompt for password"
complete -c psql -s W -l password -d "Force password prompt (should happen automatically)"
