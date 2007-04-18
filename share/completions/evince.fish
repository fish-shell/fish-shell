
complete -c evince -a '(__fish_complete_file_url)'
complete -c evince -s p -l page-label -d "The page of the document to display" -x
complete -c evince -s f -l fullscreen -d "Run evince in fullscreen mode"
complete -c evince -s s -l presentation -d "Run evince in presentation mode"
complete -c evince -s w -l preview -d "Run evince as a previewer"
complete -c evince      -l display -d "X display to use"

