
complete -c commandline -s h -l help -d "Display help and exit"
complete -c commandline -s a -l append -d "Add text to the end of the selected area"
complete -c commandline -s i -l insert -d "Add text at cursor"
complete -c commandline -s r -l replace -d "Replace selected part"

complete -c commandline -s j -l current-job -d "Select job under cursor"
complete -c commandline -s p -l current-process -d "Select process under cursor"
complete -c commandline -s s -l current-selection -d "Select current selection"
complete -c commandline -s t -l current-token -d "Select token under cursor"
complete -c commandline -s b -l current-buffer -d "Select entire command line (default)"

complete -c commandline -s c -l cut-at-cursor -d "Only return that part of the command line before the cursor"
complete -c commandline -s f -l function -d "Inject readline functions to reader"
complete -c commandline -s o -l tokenize -d "Print each token on a separate line"

complete -c commandline -s I -l input -d "Specify command to operate on"
complete -c commandline -s C -l cursor -d "Set/get cursor position, not buffer contents"
complete -c commandline -s L -l line -d "Print the line that the cursor is on"
complete -c commandline -s S -l search-mode -d "Return true if performing a history search"
complete -c commandline -s P -l paging-mode -d "Return true if showing pager content"


complete -c commandline -n '__fish_contains_opt -s f function' -a '(bind --function-names)' -d 'Function name' -x
