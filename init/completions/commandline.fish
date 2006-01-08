complete -c commandline -s a -l append -d (_ "Add text to the end of commandline")
complete -c commandline -s i -l insert -d (_ "Add text at cursor")
complete -c commandline -s r -l replace -d (_ "Replace selected part of buffer (replace)")

complete -c commandline -s j -l current-job  -d (_ "Operate only on job under cursor")
complete -c commandline -s p -l current-process  -d (_ "Operate only on process under cursor")
complete -c commandline -s t -l current-token -d (_ "Operate only on tokenizer token under cursor")
complete -c commandline -s b -l current-buffer -d (_ "Operate on entire buffer (default)")

complete -c commandline -s c -l cut-at-cursor -d (_ "Only return part of commandline before the cursor")
complete -c commandline -s f -l function -d (_ "Inject readline functions to reader")

