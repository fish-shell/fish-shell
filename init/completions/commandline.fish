complete -c commandline -s a -l append -d "Add text to the end of commandline"
complete -c commandline -s i -l insert -d "Add text at cursor"
complete -c commandline -s r -l replace -d "Replace selected part of buffer (replace)"

complete -c commandline -s j -l current-job  -d "Operate only on job under cursor"
complete -c commandline -s p -l current-process  -d "Operate only on process under cursor"
complete -c commandline -s t -l current-token -d "Operate only on tokenizer token under cursor"
complete -c commandline -s b -l current-buffer -d "Operate on entire buffer (default)"

complete -c commandline -s c -l cut-at-cursor -d "Only return part of commandline before the cursor"
complete -c commandline -s f -l function -d "Inject readline functions to reader"

