complete -c commandline -s a -l append -d (_ "Add text to the end of the selected area")
complete -c commandline -s i -l insert -d (_ "Add text at cursor")
complete -c commandline -s r -l replace -d (_ "Replace selected part")

complete -c commandline -s j -l current-job  -d (_ "Select job under cursor")
complete -c commandline -s p -l current-process  -d (_ "Select process under cursor")
complete -c commandline -s t -l current-token -d (_ "Select token under cursor")
complete -c commandline -s b -l current-buffer -d (_ "Select entire command line (default)")

complete -c commandline -s c -l cut-at-cursor -d (_ "Only return that part of the command line before the cursor")
complete -c commandline -s f -l function -d (_ "Inject readline functions to reader")

