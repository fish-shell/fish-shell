function __fish_ffplay_help_type
    printf '%s\t%s\n' long "Print more options"
    printf '%s\t%s\n' full "Print all options"

    for help_type in decoder encoder demuxer muxer filter
        set -l regex

        if test $help_type = filter
            set regex '\S+\s+(\S+)\s+\S+\s+(\S+)'
        else
            set regex '\S+\s+(\S+)\s+(\S+)'
        end

        printf '%s\n' $help_type=(ffplay -loglevel quiet -"$help_type"s | string trim | string match -rv '=|:$|^-' | string replace -rf "$regex" '$1\t$2')
    end
end

function __fish_ffplay_codec_list
    printf '%s\t%s\n' copy "Stream copy"
    printf '%s\n' (ffplay -loglevel quiet -decoders | string trim | string match -rv '=|:$|^-' | string replace -rf '\S+\s+(\S+)\s+(\S+)' '$1\t$2')
end

# Main options
complete -c ffplay -s L -d "Show license"
complete -x -c ffplay -s h -s "?" -o help -l help -a "(__fish_ffplay_help_type)" -d "Show help"
complete -c ffplay -o version -d "Show version"
complete -c ffplay -o buildconf -d "Show build configuration"
complete -c ffplay -o formats -d "Show available formats"
complete -c ffplay -o muxers -d "Show available muxers"
complete -c ffplay -o demuxers -d "Show available demuxers"
complete -c ffplay -o devices -d "Show available devices"
complete -c ffplay -o codecs -d "Show available codecs"
complete -c ffplay -o decoders -d "Show available decoders"
complete -c ffplay -o encoders -d "Show available encoders"
complete -c ffplay -o bsfs -d "Show available bit stream filters"
complete -c ffplay -o protocols -d "Show available protocols"
complete -c ffplay -o filters -d "Show available filters"
complete -c ffplay -o pix_fmts -d "Show available pixel formats"
complete -c ffplay -o layouts -d "Show standard channel layouts"
complete -c ffplay -o sample_fmts -d "Show available audio sample formats"
complete -c ffplay -o colors -d "Show available color names"
complete -x -c ffplay -o loglevel -s v -a "quiet panic fatal error warning info verbose debug trace" -d "Set logging level"
complete -c ffplay -o report -d "Generate a report"
complete -c ffplay -o max_alloc -d "Set maximum size of a single allocated block"
complete -c ffplay -o sources -d "List sources of the input device"
complete -c ffplay -o sinks -d "List sinks of the output device"
complete -c ffplay -s x -d "Force displayed width"
complete -c ffplay -s y -d "Force displayed height"
complete -c ffplay -s s -d "Set frame size"
complete -c ffplay -o fs -d "Force full screen"
complete -c ffplay -o an -d "Disable audio"
complete -c ffplay -o vn -d "Disable video"
complete -c ffplay -o sn -d "Disable subtitle"
complete -c ffplay -o ss -d "Seek to a given position in seconds"
complete -c ffplay -s t -d "Play "duration" seconds of audio/video"
complete -x -c ffplay -o bytes -a "(printf '%s\t%s\n' 0 off ; printf '%s\t%s\n' 1 on ; printf '%s\t%s\n' -1 auto)" -d "Seek by bytes"
complete -c ffplay -o nodisp -d "Disable graphical display"
complete -c ffplay -o noborder -d "Borderless window"
complete -c ffplay -o volume -d "Set startup volume"
complete -c ffplay -s f -d "Force format"
complete -c ffplay -o window_title -d "Set window title"
complete -c ffplay -o af -d "Set audio filters"
complete -x -c ffplay -o showmode -a "(printf '%s\t%s\n' 0 video ; printf '%s\t%s\n' 1 waves ; printf '%s\t%s\n' 2 RDFT)" -d "Select show mode"
complete -c ffplay -s i -d "Read specified file"
complete -x -c ffplay -o codec -a "(__fish_ffplay_codec_list)" -d "Force decoder"
complete -c ffplay -o autorotate -d "Automatically rotate video"
