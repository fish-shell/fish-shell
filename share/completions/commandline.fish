
complete -c commandline -s h -l help --description "Display help and exit"
complete -c commandline -s a -l append --description "Add text to the end of the selected area"
complete -c commandline -s i -l insert --description "Add text at cursor"
complete -c commandline -s r -l replace --description "Replace selected part"

complete -c commandline -s j -l current-job  --description "Select job under cursor"
complete -c commandline -s p -l current-process  --description "Select process under cursor"
complete -c commandline -s t -l current-token --description "Select token under cursor"
complete -c commandline -s b -l current-buffer --description "Select entire command line (default)"

complete -c commandline -s c -l cut-at-cursor --description "Only return that part of the command line before the cursor"
complete -c commandline -s f -l function --description "Inject readline functions to reader"
complete -c commandline -s o -l tokenize --description "Print each token on a separate line"

complete -c commandline -s I -l input --description "Specify command to operate on"
complete -c commandline -s C -l cursor --description "Set/get cursor position, not buffer contents"
complete -c commandline -s L -l line --description "Print the line that the cursor is on"
complete -c commandline -s S -l search-mode --description "Return true if performing a history search"
complete -c commandline -s P -l paging-mode --description "Return true if showing pager content"


complete -c commandline -n __fish_commandline_test -a '(bind --function-names)' -d 'Function name' -x
