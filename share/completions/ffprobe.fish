function __fish_ffprobe_help_type
    printf '%s\t%s\n' long "Print more options"
    printf '%s\t%s\n' full "Print all options"

    for help_type in decoder encoder demuxer muxer filter
        set -l regex

        if test $help_type = filter
            set regex '\S+\s+(\S+)\s+\S+\s+(\S+)'
        else
            set regex '\S+\s+(\S+)\s+(\S+)'
        end

        printf '%s\n' $help_type=(ffprobe -loglevel quiet -"$help_type"s | string trim | string match -rv '=|:$|^-' | string replace -rf "$regex" '$1\t$2')
    end
end

# Main options
complete -c ffprobe -s L -d "Show license"
complete -x -c ffprobe -s h -s "?" -o help -l help -a "(__fish_ffprobe_help_type)" -d "Show help"
complete -c ffprobe -o version -d "Show version"
complete -c ffprobe -o buildconf -d "Show build configuration"
complete -c ffprobe -o formats -d "Show available formats"
complete -c ffprobe -o muxers -d "Show available muxers"
complete -c ffprobe -o demuxers -d "Show available demuxers"
complete -c ffprobe -o devices -d "Show available devices"
complete -c ffprobe -o codecs -d "Show available codecs"
complete -c ffprobe -o decoders -d "Show available decoders"
complete -c ffprobe -o encoders -d "Show available encoders"
complete -c ffprobe -o bsfs -d "Show available bit stream filters"
complete -c ffprobe -o protocols -d "Show available protocols"
complete -c ffprobe -o filters -d "Show available filters"
complete -c ffprobe -o pix_fmts -d "Show available pixel formats"
complete -c ffprobe -o layouts -d "Show standard channel layouts"
complete -c ffprobe -o sample_fmts -d "Show available audio sample formats"
complete -c ffprobe -o colors -d "Show available color names"
complete -x -c ffprobe -o loglevel -s v -a "quiet panic fatal error warning info verbose debug trace" -d "Set logging level"
complete -c ffprobe -o report -d "Generate a report"
complete -c ffprobe -o max_alloc -d "Set maximum size of a single allocated block"
complete -c ffprobe -o cpuflags -d "Force specific cpu flags"
complete -c ffprobe -o hide_banner -d "Do not show program banner"
complete -c ffprobe -o sources -d "List sources of the input device"
complete -c ffprobe -o sinks -d "List sinks of the output device"
complete -c ffprobe -s f -d "Force format"
complete -c ffprobe -o unit -d "Show unit of the displayed values"
complete -c ffprobe -o prefix -d "Use SI prefixes for the displayed values"
complete -c ffprobe -o byte_binary_prefix -d "Use binary prefixes for byte units"
complete -c ffprobe -o sexagesimal -d "Use sexagesimal format HOURS:MM:SS.MICROSECONDS for time units"
complete -c ffprobe -o pretty -d "Prettify the format of displayed values, make it more human readable"
complete -x -c ffprobe -o print_format -a "default compact csv flat ini json xml" -d "Set the output printing format"
complete -x -c ffprobe -o of -a "default compact csv flat ini json xml" -d "Alias for -print_format"
complete -c ffprobe -o select_streams -d "Select the specified streams"
complete -c ffprobe -o sections -d "Print sections structure and section information, and exit"
complete -c ffprobe -o show_data -d "Show packets data"
complete -c ffprobe -o show_data_hash -d "Show packets data hash"
complete -c ffprobe -o show_error -d "Show probing error"
complete -c ffprobe -o show_format -d "Show format/container info"
complete -c ffprobe -o show_frames -d "Show frames info"
complete -c ffprobe -o show_format_entry -d "Show a particular entry from the format/container info"
complete -c ffprobe -o show_entries -d "Show a set of specified entries"
complete -c ffprobe -o show_log -d "Show log"
complete -c ffprobe -o show_packets -d "Show packets info"
complete -c ffprobe -o show_programs -d "Show programs info"
complete -c ffprobe -o show_streams -d "Show streams info"
complete -c ffprobe -o show_chapters -d "Show chapters info"
complete -c ffprobe -o count_frames -d "Count the number of frames per stream"
complete -c ffprobe -o count_packets -d "Count the number of packets per stream"
complete -c ffprobe -o show_program_version -d "Show ffprobe version"
complete -c ffprobe -o show_library_versions -d "Show library versions"
complete -c ffprobe -o show_versions -d "Show program and library versions"
complete -c ffprobe -o show_pixel_formats -d "Show pixel format descriptions"
complete -c ffprobe -o show_private_data -d "Show private data"
complete -c ffprobe -o private -d "Same as show_private_data"
complete -c ffprobe -o bitexact -d "Force bitexact output"
complete -c ffprobe -o read_intervals -d "Set read intervals"
complete -c ffprobe -o default -d "Generic catch all option"
complete -c ffprobe -s i -d "Read specified file"
complete -c ffprobe -o find_stream_info -d "Read and decode the streams to fill missing information with heuristics"
