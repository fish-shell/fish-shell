function __fish_ffmpeg_last_arg
    echo (commandline -cx)[-1]
end

# Allow completions to match against an argument that includes a stream specifier, e.g. -c:v:2
function __fish_ffmpeg_complete_regex
    set -l regex $argv[1]
    set -l completions $argv[2..-1]

    complete -x -c ffmpeg -n "__fish_ffmpeg_last_arg | string match -rq -- '^'\"$regex\"'(\$|:)'" \
        -a "$completions"
end

function __fish_ffmpeg_help_type
    printf '%s\t%s\n' long "Print more options"
    printf '%s\t%s\n' full "Print all options"

    for help_type in decoder encoder demuxer muxer filter
        set -l regex

        if test $help_type = filter
            set regex '\S+\s+(\S+)\s+\S+\s+(\S+)'
        else
            set regex '\S+\s+(\S+)\s+(\S+)'
        end

        printf '%s\n' $help_type=(ffmpeg -loglevel quiet -"$help_type"s | string trim \
            | string match -rv '=|:$|^-' | string replace -rf "$regex" '$1\t$2')
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
        case all
            set identifier '.*'
        case '*'
            return 1
    end

    printf '%s\n' (ffmpeg -loglevel quiet -decoders | string trim | string match -rv '=|:$|^-' \
        | string match -r "$identifier" | string replace -rf '\S+\s+(\S+)\s+(\S+)' '$1\t$2')
end

function __fish_ffmpeg_pix_fmts
    # We can't know in advance if the intention is to specify an output pix_fmt because the intent
    # could be to instead provide a second input, but we can rule out an output if no input has
    # been specified
    set -l regex_filter '.'
    if contains -- -i (commandline -cx)
        set regex_filter '^I'
    end
    ffmpeg -hide_banner -loglevel quiet -pix_fmts |
        # tail -n +9 | # skip past header
        string match -rv '[:=-]|FLAGS' | # skip past header
        string match -er $regex_filter |
        string replace -rf '^[IOHPB.]{5} (\S+) .*' '$1'
end

function __fish_ffmpeg_filters
    # TODO: Figure out how to distinguish {audio,video} source and destination, because some filters
    # can go cross those lines (e.g. showvolume, which generates a video representation of the input
    # audio).

    set -l filter '.'
    switch $argv[1]
        case video
            set filter '[V]\S*->|->\S*[V]'
        case audio
            set filter '[A]\S*->|->\S*[A]'
        case all
        case '*'
            set filter '.'
    end

    ffmpeg -hide_banner -loglevel quiet -filters |
        string match -e -- '->' | # skip past the header
        string match -er $filter |
        string replace -rf '^ [TSC.]{3} +(\S+) +\S+->\S+ +(.*)' '$1\t$2'
end

function __fish_ffmpeg_presets
    set -l cmdline (commandline)
    if string match -ireq '[hx]26[45]'
        printf "%s\n" ultrafast superfast veryfast faster fast medium slow slower veryslow placebo
    end
end

function __fish_ffmpeg_tunes
    set -l cmdline (commandline)
    if string match -req 264
        printf "%s\n" film animation grain stillimage fastdecode zerolatency psnr ssim
    end
    if string match -req '265|hevc'
        printf "%s\n" psnr ssim grain zerolatency fastdecode
    end
end

function __fish_ffmpeg_crfs
    seq 1 30
end

function __fish_ffmpeg_profile
    if string match -req 264
        printf "%s\n" baseline main high
    end
    if string match -req '265|hevc'
        printf "%s\n" main high
    end
end

function __fish_ffmpeg_formats
    # TODO: Use heuristic to determine input vs output format and filter accordingly
    ffmpeg -hide_banner -loglevel quiet -formats | string replace -rf '^ [DE.]{2} ([a-z0-9_]+) +(\S.+)$' '$1\t$2'
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
complete -x -c ffmpeg -o loglevel -s v \
    -a "quiet panic fatal error warning info verbose debug trace" -d "Set logging level"
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
complete -c ffmpeg -s f -d "Force format" -xa "(__fish_ffmpeg_formats)"
complete -c ffmpeg -s c -o codec -d "Codec name" -xa "(__fish_ffmpeg_codec_list all)"
complete -c ffmpeg -o map_metadata -d "Set metadata information of outfile from infile"
complete -c ffmpeg -s t -d "Record or transcode \"duration\" seconds of audio/video"
complete -c ffmpeg -o to -d "Record or transcode stop time"
complete -c ffmpeg -o fs -d "Set the limit file size in bytes"
complete -c ffmpeg -o ss -d "Set the start time offset"
complete -c ffmpeg -o seek_timestamp -d "Enable/disable seeking by timestamp with -ss"
complete -c ffmpeg -o timestamp -d "Set the recording timestamp"
complete -c ffmpeg -o metadata -d "Add metadata"
complete -c ffmpeg -o program -d "Add program with specified streams"
complete -x -c ffmpeg -o target -a '(for target in vcd svcd dvd dv dv50 ; echo "$target" ; \
    echo pal-"$target" ; echo ntsc-"$target" ; echo film-"$target" ; end)' \
    -d "Specify target file type"
complete -c ffmpeg -o apad -d "Audio pad"
complete -c ffmpeg -o frames -d "Set the number of frames to output"
complete -c ffmpeg -o filter -d "Set stream filtergraph"
complete -c ffmpeg -o filter_script -d "Read stream filtergraph description from a file"
complete -c ffmpeg -o reinit_filter -d "Reinit filtergraph on input parameter changes"
complete -c ffmpeg -o discard -d Discard
complete -c ffmpeg -o disposition -d Disposition

# Video options
complete -c ffmpeg -o vframes -d "Set the number of video frames to output"
complete -c ffmpeg -s r -d "Set frame rate"
complete -c ffmpeg -s s -d "Set frame size"
complete -c ffmpeg -o aspect -d "Set aspect ratio"
complete -c ffmpeg -o bits_per_raw_sample -d "Set the number of bits per raw sample"
complete -c ffmpeg -o vn -d "Disable video"
complete -c ffmpeg -o vcodec -o "codec:v" -o "c:v"
# Also list codecs when a particular stream is selected, e.g. -c:v:0
__fish_ffmpeg_complete_regex "-(vcodec|c(odec)?:v)" "(__fish_ffmpeg_codec_list video)"
complete -c ffmpeg -o timecode -d "Set initial TimeCode value"
complete -x -c ffmpeg -o pass -a "1 2 3" -d "Select the pass number"
complete -c ffmpeg -o vf -d "Set video filters"
complete -c ffmpeg -o ab -o "b:a" -d "Audio bitrate"
complete -c ffmpeg -s b -o "b:v" -d "Video bitrate"
complete -c ffmpeg -o dn -d "Disable data"
# Advanced video options
complete -c ffmpeg -o pix_fmt
__fish_ffmpeg_complete_regex -pix_fmt "(__fish_ffmpeg_pix_fmts)"

# Audio options
complete -c ffmpeg -o aframes -d "Set the number of audio frames to output"
complete -c ffmpeg -o aq -d "Set audio quality"
complete -c ffmpeg -o ar -d "Set audio sampling rate"
complete -c ffmpeg -o ac -d "Set number of audio channels"
complete -c ffmpeg -o an -d "Disable audio"
complete -c ffmpeg -o acodec -o "codec:a" -o "c:a"
__fish_ffmpeg_complete_regex '-(acodec|c(odec)?:a)' "(__fish_ffmpeg_codec_list audio)"
complete -c ffmpeg -o vol -d "Change audio volume"
complete -c ffmpeg -o af -d "Set audio filters"

# Subtitle options
complete -c ffmpeg -s s -d "Set frame size"
complete -c ffmpeg -o sn -d "Disable subtitle"
complete -c ffmpeg -o scodec -o "codec:s" -o "c:s"
__fish_ffmpeg_complete_regex '-(scodec|c(odec)?:s)(:\d+)?' "(__fish_ffmpeg_codec_list subtitle)"
complete -c ffmpeg -o stag -d "Force subtitle tag/fourcc"
complete -c ffmpeg -o fix_sub_duration -d "Fix subtitles duration"
complete -c ffmpeg -o canvas_size -d "Set canvas size"
complete -c ffmpeg -o spre -d "Set the subtitle options to the indicated preset"

# Codec-specific options
complete -c ffmpeg -o pre -o preset -d "Preset name"
__fish_ffmpeg_complete_regex 'pre(set)?' "(__fish_ffmpeg_presets)"
complete -c ffmpeg -o tune
__fish_ffmpeg_complete_regex tune "(__fish_ffmpeg_tunes)"
complete -c ffmpeg -o crf -o q
__fish_ffmpeg_complete_regex 'crf|q' "(__fish_ffmpeg_crfs)"
complete -c ffmpeg -o profile
__fish_ffmpeg_complete_regex profile "(__fish_ffmpeg_profiles)"

# Filters
#
# fish completions are not designed to take the current argument being completed as an input,
# and if introspected it is normally used to filter/constrain the completions generated. ffmpeg's
# command line syntax is extremely nasty and in addition to using stringification for all arguments,
# relies on special characters to delimit separate subarguments (rather than, e.g., concatenating
# repeated invocations of the same switch), meaning the typical way of generating fish completions
# would only allow us to generate the very first subargument in a complex argument payload.
#
# We hack around this in a ridiculously ugly fashion by always generating completions that are
# prepended with a prefix of the current value of the payload in question.
#
# Top-level ffmpeg filters are comma separated, but take optional arguments that might themselves
# take optional subarguments. While = is a key=value delimiter, it is also a prefix for the first
# argument to a filter, e.g. filter_name=opt1 and filter_name=opt1=opt_value, and in the case that
# a filter takes more than one argument, filter arguments are delimited with :
#
# A complete example:
#
# ffmpeg .... -filter:v filter1,filter2=f2_arg1=f2_arg1_val:f2_arg2

function __fish_ffmpeg_split_filter_graph
    set -l filters (string split , $argv[1])
    printf "%s\n" $filters
end

# Given a single filter expression, emits the filter name on the first line and then each key=value
# pair of arguments to the filter on each subsequent line.
function __fish_ffmpeg_decompose_filter
    set -l parts (string split -m 1 = "$argv")
    echo $parts[1] # the filter name
    set -l arguments (string split : $parts[2])
    printf "%s\n" $arguments
end

function __fish_ffmpeg_concat_filters
    string join -- , $argv
end

function __fish_ffmpeg_concat_filter_args
    string join -- : $argv
end

function __fish_ffmpeg_complete_filter
    set -l filter_type all
    if string match -rq -- '^-(vf(ilter)?|f(ilter)?:v)' (__fish_ffmpeg_last_arg)
        set filter_type video
    else if string match -rq -- '^-(af(ilter)?|f(ilter)?:a)' (__fish_ffmpeg_last_arg)
        set filter_type audio
    end

    # echo -e "\n **** $filter_type **** \n" > /dev/tty

    set -l filters_arg (commandline -xp)[-1]
    if string match -rq -- '^-' $filters_arg
        # No filter name started
        __fish_ffmpeg_filters $filter_type
        return
    end

    set -l filters (__fish_ffmpeg_split_filter_graph $filters_arg)
    # We are completing only the last filter (in case there are multiple)
    set -l filter $filters[-1]
    # Remove it from the list of passed-through filters
    set -e filters[-1]
    # `ffmpeg -h filter=FILTER` exposes information that can be used to dynamically complete not
    # only the name of the filter but also the name of its individual options. We currently only
    # support completing the name of the filter.
    set -l decomposed (__fish_ffmpeg_decompose_filter $filter)
    set -l filter_name $decomposed[1]
    set -l filter_args (__fish_ffmpeg_concat_filter_args $decomposed[2..-1])

    # Emit the mutated filter graph, with permutations of the final filter name offered as
    # completions.
    for known_filter in (__fish_ffmpeg_filters $filter_type)
        set -l modified_filter (string join = $known_filter $filter_args)
        __fish_ffmpeg_concat_filters $filters $modified_filter
    end
end

complete -x -c ffmpeg -o filter -o filter:v -o filter:s -o filter:a -o vf -o af \
    -a "(__fish_ffmpeg_complete_filter)"
