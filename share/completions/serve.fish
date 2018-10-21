# See: https://github.com/zeit/serve

complete -x -c serve -l help -s h -d "Shows help message"
complete -x -c serve -l version -s v -d "Displays the current version of serve"
complete -x -c serve -l listen -s l -d "Specify a URI endpoint on which to listen"
complete -x -c serve -l debug -s d -d "Show debugging information"
complete -x -c serve -l single -s s -d "Rewrite all not-found requests to `index.html`"
complete -x -c serve -l config -s c -d "Specify custom path to `serve.json`"
complete -x -c serve -l no-clipboard -s n -d "Do not copy the local address to the clipboard"
