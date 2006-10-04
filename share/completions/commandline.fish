complete -c commandline -s a -l append -d (N_ "Add text to the end of the selected area")
complete -c commandline -s i -l insert -d (N_ "Add text at cursor")
complete -c commandline -s r -l replace -d (N_ "Replace selected part")

complete -c commandline -s j -l current-job  -d (N_ "Select job under cursor")
complete -c commandline -s p -l current-process  -d (N_ "Select process under cursor")
complete -c commandline -s t -l current-token -d (N_ "Select token under cursor")
complete -c commandline -s b -l current-buffer -d (N_ "Select entire command line (default)")

complete -c commandline -s c -l cut-at-cursor -d (N_ "Only return that part of the command line before the cursor")
complete -c commandline -s f -l function -d (N_ "Inject readline functions to reader")

complete -c commandline -s I -l input -d (N_ "Specify command to operate on")
complete -c commandline -s C -l cursor -d (N_ "Set/get cursor position, not buffer contents")

