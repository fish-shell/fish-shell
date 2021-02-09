complete -c msfconsole -f

# Common options
complete -c msfconsole -s E -l environment -x -d 'Set Rails environment'

# Database options
complete -c msfconsole -s M -l migration-path -xa "(__fish_complete_directories)" -d 'Directory containing additional DB migrations'
complete -c msfconsole -s n -l no-database -d 'Disable database support'
complete -c msfconsole -s y -l yaml -F -d 'YAML file containing database settings'

# Framework options
complete -c msfconsole -s c -F -d 'Load the specified configuration file'
complete -c msfconsole -s v -s V -l version -d 'Show version'

# Module options
complete -c msfconsole -l defer-module-loads -d 'Defer module loading'
complete -c msfconsole -s m -l module-path -xa "(__fish_complete_directories)" -d 'Load an additional module path'

# Console options
complete -c msfconsole -s a -l ask -d 'Ask before exiting Metasploit'
complete -c msfconsole -s H -l history-file -F -d 'Save command history to the specified file'
complete -c msfconsole -s l -l logger -xa 'Stdout Flatfile StdoutWithoutTimestamps TimestampColorlessFlatfile Stderr' -d 'Specify a logger to use'
complete -c msfconsole -s L -l real-readline -d 'Use the system Readline library'
complete -c msfconsole -s o -l output -F -d 'Output to the specified file'
complete -c msfconsole -s p -l plugin -x -d 'Load a plugin on startup'
complete -c msfconsole -s q -l quiet -d 'Do not print the banner on startup'
complete -c msfconsole -s r -l resource -F -d 'Execute the specified resource file'
complete -c msfconsole -s x -l execute-command -x -d 'Execute the specified console commands'
complete -c msfconsole -s h -l help -d 'Show help message'
