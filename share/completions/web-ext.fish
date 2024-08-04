complete -c web-ext -s h -l help -d 'Show help'
complete -c web-ext -l version -d 'Show version'

complete -c web-ext -a build -d "Create the extension package from source"
complete -c web-ext -a dump-config -d "Run the config discovery and dump the resulting config data as JSON"
complete -c web-ext -a sign -d "Sign the extension so it can be installed in Firefox"
complete -c web-ext -a run -d "Run the extension"
complete -c web-ext -a lint -d "Validate the extension source"
complete -c web-ext -a docs -d "Show help in the browser"

complete -c web-ext -s s -l source-dir -d "Set the extension source directory"
complete -c web-ext -s a -l artifacts-dir -d "Set the directory where artifacts will be saved"
complete -c web-ext -s v -l verbose -d "Show verbose output"
complete -c web-ext -s i -l ignore-files -d "List glob patterns to define which files should be ignored"
complete -c web-ext -l no-input -d "Disable all features that require standard input"
complete -c web-ext -s c -l config -d "Set the path to a CommonJS config file to set option defaults"
complete -c web-ext -l config-discovery -d "Discover the config files in home directory and working directory"
