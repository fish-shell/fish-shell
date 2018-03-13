# Completions for JBake (https://jbake.org/)

complete -c jbake -o b -l bake -f -d "Performs a bake"
complete -c jbake -o h -l help -f -d "Prints the help message"
complete -c jbake -o i -l init -d "Initialises the required directory structure with default templates"
complete -c jbake -o s -l server -d "Runs the HTTP server to serve out the baked site"
complete -c jbake -o t -l template -x -d "Uses the specified template engine for default templates"
