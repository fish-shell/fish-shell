# fish completion for t-rec (https://github.com/sassman/t-rec-rs

function __fish_t_rec_time_unit
    set -l cur (commandline --current-token)
    if string match -qr '^\d+$' -- $cur
        echo $cur"ms"\t"milliseconds"
        echo $cur"s"\t"seconds"
        echo $cur"m"\t"minutes"
    end
end

function __fish_t_rec_window_list
    string replace -r -- '\s*(.*)\|\s+(\d+)' '$2\t$1' (t-rec --ls | tail -n +2)
end

# Options
complete -c t-rec -d "Command to run instead of shell" -xa "(complete -C '' | string split \t -f1)"
complete -c t-rec -s v -l verbose -d "Enable verbose insights for the curious"
complete -c t-rec -s q -l quiet -d "Quiet mode, suppresses the banner"
complete -c t-rec -s m -l video -d "Generate both gif and mp4 video"
complete -c t-rec -s M -l video-only -d "Generate only mp4 video, not gif"
complete -c t-rec -s d -l decor -d "Decorate animation" -xa "shadow none"
complete -c t-rec -s b -l bg -d "Background color when decors are used" -xa "white black transparent"
complete -c t-rec -s n -l natural -d "Natural typing, disables idle detection and sampling optimization"
complete -c t-rec -s l -l ls -d "List windows available for recording by their id"
complete -c t-rec -s w -l win-id -d "Id of window to capture" -xa "(__t_rec_window_list)"
complete -c t-rec -s e -l end-pause -d "Pause time at end of animation" -xa "(__fish_t_rec_time_unit)" -r
complete -c t-rec -s s -l start-pause -d "Pause time at start of animation" -xa "(__fish_t_rec_time_unit)" -r
complete -c t-rec -s o -l output -d "Output file (without extension); defaults to t-rec" -r
complete -c t-rec -s h -l help -d "Print help"
complete -c t-rec -s V -l version -d "Print version"
