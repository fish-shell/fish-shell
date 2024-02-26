function __fish_evince_complete_file_url
    set -l comp

    if set -q argv[1]
        set comp $argv[1]
    else
        set comp (commandline -ct)
    end

    set -l prefix (echo $comp|cut -c 1-7)

    if test file:// = $prefix
        set -l stripped (echo $comp|cut -c 8-)
        printf "%s\n" file://(complete -C"echo $stripped")
    else
        echo file://
    end

end

complete -c evince -a '(__fish_evince_complete_file_url)'
complete -c evince -s p -l page-label -d "The page of the document to display" -x
complete -c evince -s f -l fullscreen -d "Run evince in fullscreen mode"
complete -c evince -s s -l presentation -d "Run evince in presentation mode"
complete -c evince -s w -l preview -d "Run evince as a previewer"
complete -c evince -l display -d "X display to use"
