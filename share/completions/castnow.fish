# Castnow is a utility that can be used to play back media files on Chromecast devices.
# See: https://github.com/xat/castnow

set -l __fish_castnow_keys "space\tToggle\ between\ play\ and\ pause m\tToggle\ mute t\tToggle\ subtitles up\tVolume\ up down\tVolume\ down left\tSeek\ backward right\tSeek\ forward n\tNext\ in\ playlist s\tStop\ playback quit\tQuit"

complete -c castnow -l tomp4 -d "Convert file to mp4 during playback"
complete -c castnow -l device -d "Specify name of Chromecast device to be used" -x
complete -c castnow -l address -d "Specify IP or hostname of Chromecast device" -x
complete -c castnow -l subtitles -d "Path or URL to SRT or VTT file" -k -x -a "(__fish_complete_suffix .srt .vtt)"
complete -c castnow -l subtitles-scale -d "Set subtitles font scale" -x
complete -c castnow -l subtitles-color -d "Set subtitles font RGBA color" -x
complete -c castnow -l subtitles-port -d "Specify port to be used for serving subtitles" -x
complete -c castnow -l myip -d "Set local IP" -x
complete -c castnow -l quiet -d "No output"
complete -c castnow -l type -d "Explicitly set the mime-type" -a "(__fish_print_xdg_mimetypes | string match -r '^video/.*')" -x
complete -c castnow -l bypass-srt-encoding -d "Disable automatic UTF-8 encoding of SRT subtitles"
complete -c castnow -l seek -d "Seek to specified time (format: [hh:]mm:ss)" -x
complete -c castnow -l loop -d "Loop over playlist, or file"
complete -c castnow -l shuffle -d "Play in random order"
complete -c castnow -l recursive -d "List all files in directories recursively"
complete -c castnow -l volume-step -d "Set at which the volume changes" -x
complete -c castnow -l localfile-port -d "Specify port to be used for serving local file" -x
complete -c castnow -l transcode-port -d "Specify port to be used for serving transcoded file" -x
complete -c castnow -l torrent-port -d "Specify port to be used for serving torrented file" -x
complete -c castnow -l stdin-port -d "Specify port to be used for serving file read from stdin" -x
complete -c castnow -l command -d "Execute key command(s)" -x -a "(__fish_append , $__fish_castnow_keys)"
complete -c castnow -l exit -d "Exit when playback begins or --command completes"
complete -c castnow -l help -d "Display help"
