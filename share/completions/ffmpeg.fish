function __fish_ffmpeg_help_type
    printf '%s\t%s\n' long "Print more options"
    printf '%s\t%s\n' full "Print all options"

    for help_type in decoder encoder demuxer muxer filter
        set -l regex

        if test $help_type = "filter"
            set regex '\S+\s+(\S+)\s+\S+\s+(\S+)'
        else
            set regex '\S+\s+(\S+)\s+(\S+)'
        end

        printf '%s\n' $help_type=(ffmpeg -loglevel quiet -"$help_type"s | string trim | string match -rv '=|:$|^-' | string replace -rf "$regex" '$1\t$2')
    end
end

function __fish_ffmpeg_codec_list
    printf '%s\t%s\n' copy "Stream copy"

    set -l identifier

    switch $argv[1]
        case video
            set identifier '^V.*'
        case audio
            set identifier '^A.*'
        case subtitle
            set identifier '^S.*'
        case '*'
            return 1
    end

    printf '%s\n' (ffmpeg -loglevel quiet -decoders | string trim | string match -rv '=|:$|^-' | string match -r "$identifier" | string replace -rf '\S+\s+(\S+)\s+(\S+)' '$1\t$2')
end

complete -c ffmpeg -s i -d "Specify input file"

# Print help / information / capabilities
complete -c ffmpeg -s L -d "Show license"
complete -x -c ffmpeg -s h -s "?" -o help -l help -a "(__fish_ffmpeg_help_type)" -d "Show help"
complete -c ffmpeg -o version -d "Show version"
complete -c ffmpeg -o buildconf -d "Show build configuration"
complete -c ffmpeg -o formats -d "Show available formats"
complete -c ffmpeg -o muxers -d "Show available muxers"
complete -c ffmpeg -o demuxers -d "Show available demuxers"
complete -c ffmpeg -o devices -d "Show available devices"
complete -c ffmpeg -o codecs -d "Show available codecs"
complete -c ffmpeg -o decoders -d "Show available decoders"
complete -c ffmpeg -o encoders -d "Show available encoders"
complete -c ffmpeg -o bsfs -d "Show available bit stream filters"
complete -c ffmpeg -o protocols -d "Show available protocols"
complete -c ffmpeg -o filters -d "Show available filters"
complete -c ffmpeg -o pix_fmts -d "Show available pixel formats"
complete -c ffmpeg -o layouts -d "Show standard channel layouts"
complete -c ffmpeg -o sample_fmts -d "Show available audio sample formats"
complete -c ffmpeg -o colors -d "Show available color names"
complete -c ffmpeg -o sources -d "List sources of the input device"
complete -c ffmpeg -o sinks -d "List sinks of the output device"
complete -c ffmpeg -o hwaccels -d "Show available HW acceleration methods"

# Global options
complete -x -c ffmpeg -o loglevel -s v -a "quiet panic fatal error warning info verbose debug trace" -d "Set logging level"
complete -c ffmpeg -o report -d "Generate a report"
complete -c ffmpeg -o max_alloc -d "Set maximum size of a single allocated block"
complete -c ffmpeg -s y -d "Overwrite output files"
complete -c ffmpeg -s n -d "Never overwrite output files"
complete -c ffmpeg -o ignore_unknown -d "Ignore unknown stream types"
complete -c ffmpeg -o filter_threads -d "Number of non-complex filter threads"
complete -c ffmpeg -o filter_complex_threads -d "Number of threads for -filter_complex"
complete -c ffmpeg -o stats -d "Print progress report during encoding"
complete -c ffmpeg -o max_error_rate -d "Ratio of errors"
complete -c ffmpeg -o bits_per_raw_sample -d "Set the number of bits per raw sample"
complete -c ffmpeg -o vol -d "Change audio volume"

# Per-file main options
complete -c ffmpeg -s f -d "Force format"
complete -c ffmpeg -s c -o codec -d "Codec name"
complete -c ffmpeg -o pre -d "Preset name"
complete -c ffmpeg -o map_metadata -d "Set metadata information of outfile from infile"
complete -c ffmpeg -s t -d "Record or transcode \"duration\" seconds of audio/video"
complete -c ffmpeg -o to -d "Record or transcode stop time"
complete -c ffmpeg -o fs -d "Set the limit file size in bytes"
complete -c ffmpeg -o ss -d "Set the start time offset"
complete -c ffmpeg -o seek_timestamp -d "Enable/disable seeking by timestamp with -ss"
complete -c ffmpeg -o timestamp -d "Set the recording timestamp"
complete -c ffmpeg -o metadata -d "Add metadata"
complete -c ffmpeg -o program -d "Add program with specified streams"
complete -x -c ffmpeg -o target -a '(for target in vcd svcd dvd dv dv50 ; echo "$target" ; echo pal-"$target" ; echo ntsc-"$target" ; echo film-"$target" ; end)' -d "Specify target file type"
complete -c ffmpeg -o apad -d "Audio pad"
complete -c ffmpeg -o frames -d "Set the number of frames to output"
complete -c ffmpeg -o filter -d "Set stream filtergraph"
complete -c ffmpeg -o filter_script -d "Read stream filtergraph description from a file"
complete -c ffmpeg -o reinit_filter -d "Reinit filtergraph on input parameter changes"
complete -c ffmpeg -o discard -d "Discard"
complete -c ffmpeg -o disposition -d "Disposition"

# Video options
complete -c ffmpeg -o vframes -d "Set the number of video frames to output"
complete -c ffmpeg -s r -d "Set frame rate"
complete -c ffmpeg -s s -d "Set frame size"
complete -c ffmpeg -o aspect -d "Set aspect ratio"
complete -c ffmpeg -o bits_per_raw_sample -d "Set the number of bits per raw sample"
complete -c ffmpeg -o vn -d "Disable video"
complete -x -c ffmpeg -o vcodec -o "codec:v" -o "c:v" -a "(__fish_ffmpeg_codec_list video)" -d "Set video codec"
complete -c ffmpeg -o timecode -d "Set initial TimeCode value"
complete -x -c ffmpeg -o pass -a "1 2 3" -d "Select the pass number"
complete -c ffmpeg -o vf -d "Set video filters"
complete -c ffmpeg -o ab -o "b:a" -d "Audio bitrate"
complete -c ffmpeg -s b -o "b:v" -d "Video bitrate"
complete -c ffmpeg -o dn -d "Disable data"

# Audio options
complete -c ffmpeg -o aframes -d "Set the number of audio frames to output"
complete -c ffmpeg -o aq -d "Set audio quality"
complete -c ffmpeg -o ar -d "Set audio sampling rate"
complete -c ffmpeg -o ac -d "Set number of audio channels"
complete -c ffmpeg -o an -d "Disable audio"
complete -x -c ffmpeg -o acodec -o "codec:a" -o "c:a" -a "(__fish_ffmpeg_codec_list audio)" -d "Set audio codec"
complete -c ffmpeg -o vol -d "Change audio volume"
complete -c ffmpeg -o af -d "Set audio filters"

# Subtitle options
complete -c ffmpeg -s s -d "Set frame size"
complete -c ffmpeg -o sn -d "Disable subtitle"
complete -x -c ffmpeg -o scodec -o "codec:s" -o "c:s" -a "(__fish_ffmpeg_codec_list subtitle)" -d "Set subtitle codec"
complete -c ffmpeg -o stag -d "Force subtitle tag/fourcc"
complete -c ffmpeg -o fix_sub_duration -d "Fix subtitles duration"
complete -c ffmpeg -o canvas_size -d "Set canvas size"
complete -c ffmpeg -o spre -d "Set the subtitle options to the indicated preset"
