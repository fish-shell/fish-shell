complete -c zed -s w -l wait -d "Wait for all of the given paths to be opened/closed before exiting"
complete -c zed -s a -l add -d "Add files to the currently open workspace"
complete -c zed -s n -l new -d "Create a new workspace"
complete -c zed -s v -l version -d "Print Zed's version and the app path"
complete -c zed -l foreground -d "Run zed in the foreground (useful for debugging)"
complete -c zed -l zed -d "Custom path to Zed.app or the zed binary" -r
complete -c zed -l dev-server-token -d "Run zed in dev-server mode" -r
complete -c zed -l uninstall -d "Uninstall Zed from user system"
complete -c zed -s h -l help -d "Print help (see a summary with '-h')"
