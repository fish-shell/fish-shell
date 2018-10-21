# Completions for JBake (https://jbake.org/)

complete -c jbake -o b -l bake -f -d "Perform a bake"
complete -c jbake -o h -l help -f -d "Print the help message"
complete -c jbake -o i -l init -d "Initialise the required directory structure with default templates"
complete -c jbake -o s -l server -d "Run the HTTP server to serve out the baked site"
complete -c jbake -o t -l template -x -d "Use the specified template engine for default templates"
