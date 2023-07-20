function __krita_complete_image_format -d 'Function to generate image format completion'
    set -l previous_token (commandline -oc)[-1]
    set -l current_token (commandline -opc)

    if test $previous_token = --new-image
        switch $current_token
            case '*,*,*'
                # nothing is completed as arbitrary width and height are expected
            case '*,'
                echo -e 'U8
U16
F16
F32' | awk "{ printf \"$current_token%s\n\", \$1 }"
            case '*'
                echo -e 'RGBA
                XYZA
                LABA
                CMYKA
                GRAY
                YCbCrA'
        end
    end
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
complete -c krita -l workspace -d 'open with workspace' -a 'Animation Big_Paint_2 Big_Paint Big_Vector Default Minimal Small_Vector Storyboarding VFX_Paint' -x
complete -c krita -l file-layer -d 'open with file-layer' -r
complete -c krita -l resource-location -d 'open with resource' -r

complete -c krita -l new-image -d 'open with new image'
complete -c krita -a '(__krita_complete_image_format)' -x
