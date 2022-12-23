set -l options (mpv --list-options 2>/dev/null)
complete -c mpv -l(string replace -fr '^\s*--([\w-]+).*' '$1' -- $options)
complete -c mpv -lno-(string replace -fr '^\s*--([\w-]+).*Flag.*' '$1' -- $options)

complete -c mpv -l start -x -d "Seek to given position (%, s, hh:mm:ss)"
complete -c mpv -l no-audio -d "Disable audio"
complete -c mpv -l no-audio-display -d "Hide attached picture for audio"
complete -c mpv -l no-video -d "Disable video"
complete -c mpv -l fs -d "Fullscreen playback"
complete -c mpv -l sub-file -r -d "Specify subtitle file"
complete -c mpv -l playlist -r -d "Specify playlist file"
