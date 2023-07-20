complete -c krita -s h -l help -d 'show help'
complete -c krita -l help-all -d 'show help with Qt options'
complete -c krita -s v -l version -d 'show version'

complete -c krita -l export -d 'export file as image'
complete -c krita -l export-pdf -d 'export file as PDF'
complete -c krita -l export-sequence -d 'export animation as sequence'

complete -c krita -l export-filename -d 'exported filename' -n '__fish_seen_subcommand_from --export --export-pdf --export-sequence' -r

complete -c krita -l template -d 'open template' -r

complete -c krita -l nosplash -d 'hide splash screen'
complete -c krita -l canvasonly -d 'open with canvasonly mode'
complete -c krita -l fullscreen -d 'open with fullscreen mode'
complete -c krita -l workspace -d 'open with workspace' -a 'Animation Big_Paint_2 Big_Paint Big_Vector Default Minimal Small_Vector Storyboarding VFX_Paint' -x
complete -c krita -l file-layer -d 'open with file-layer' -r
