function __krita_complete_image_format
    set -l previous_token (commandline -xc)[-1]
    set -l current_token (commandline -t)

    if test "$previous_token" = --new-image
        switch $current_token
            case '*,*,*'
                # nothing is completed as arbitrary width and height are expected
            case '*,'
                printf '%s,\n' U8 U16 F16 F32 |
                    string replace -r '^' $current_token
            case '*'
                printf '%s,\n' RGBA XYZA LABA CMYKA GRAY YCbCrA
        end
    end
end

function __krita_list_workspaces
    path basename ~/.local/share/krita/workspaces/*.kws |
        path change-extension ''
end

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
complete -c krita -l workspace -d 'open with workspace' -a '(__krita_list_workspaces)' -x
complete -c krita -l file-layer -d 'open with file-layer' -r
complete -c krita -l resource-location -d 'open with resource' -r

complete -c krita -l new-image -d 'open with new image'
complete -c krita -a '(__krita_complete_image_format)' -x
