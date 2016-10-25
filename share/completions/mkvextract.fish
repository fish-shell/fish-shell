# Sample output of 'mkvmerge -i file.mkv'
# 
# File 'file.mkv': container: Matroska
# Track ID 0: video (MPEG-4p10/AVC/h.264)
# Track ID 1: audio (AAC)
# Track ID 2: subtitles (SubStationAlpha)
# Attachment ID 1: type 'application/x-truetype-font', size 53532 bytes, file name 'some_font.ttf'
# Chapters: 7 entires

function __fish_print_mkvextract_attachments
    mkvmerge -i $argv | string match 'Attachment ID*' | string replace -r '.*?(\d+).*? type \'(.*?)\'.*?file name \'(.*?)\'' '$1:\t$3 ($2)'
end

function __fish_print_mkvextract_tracks
    mkvmerge -i $argv | string match 'Track ID*' | string replace -r '.*?(\d+): (.*)' '$1:\t$2'
end

# simple options
complete -f -c mkvextract -n "test (count (commandline -opc)) -lt 2"    -s V -l "version" -d "Show version information"
complete -f -c mkvextract -n "test (count (commandline -opc)) -lt 2"    -s h -l "help"    -d "Show help"
# commands
complete -f -c mkvextract -n "test (count (commandline -opc)) -lt 2" -a "tracks"       -d "Extract tracks to external files"
complete -f -c mkvextract -n "test (count (commandline -opc)) -lt 2" -a "tags"         -d "Extract tags as XML"
complete -f -c mkvextract -n "test (count (commandline -opc)) -lt 2" -a "attachments"  -d "Extract attachments"
complete -f -c mkvextract -n "test (count (commandline -opc)) -lt 2" -a "chapters"     -d "Extract chapters as XML"
complete -f -c mkvextract -n "test (count (commandline -opc)) -lt 2" -a "cuesheet"     -d "Extract chapters and tags as CUE sheet"
complete -f -c mkvextract -n "test (count (commandline -opc)) -lt 2" -a "timecodes_v2" -d "Extract timecodes of a track as timecode v2 file"
complete -f -c mkvextract -n "test (count (commandline -opc)) -lt 2" -a "cues"         -d "Extract cue information as text file"
# dynamic tracks and attachments completions
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3; and contains -- (commandline -opc)[2] tracks timecodes_v2 cues" -a "(__fish_print_mkvextract_tracks (commandline -opc)[3])"
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3; and contains -- (commandline -opc)[2] attachments"              -a "(__fish_print_mkvextract_attachments (commandline -opc)[3])"
# options common to all commands
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3"    -s f -l "parse-fully"          -d "Parse the whole file instead of relying on the index"
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3"    -s v -l "verbose"              -d "Increase verbosity"
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3"    -s q -l "quiet"                -d "Suppress status output"
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3" -r      -l "ui-language"          -d "Force the translations for 'code' to be used"
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3" -r      -l "command-line-charset" -d "Charset for strings on the command line"
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3" -r      -l "output-charset"       -d "Outputs messages in specified charset"
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3" -r -s r -l "redirect-output"      -d "Redirect all messages into a file"
# command-specific options
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3; and contains -- (commandline -opc)[2] tracks"   -r -s c                      -d "Convert text subtitles to a charset"
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3; and contains -- (commandline -opc)[2] tracks"           -l "cuesheet"        -d "Also try to extract the CUE sheet"
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3; and contains -- (commandline -opc)[2] tracks"   -r      -l "blockadd"        -d "Keep only the BlockAdditions up to the specified level"
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3; and contains -- (commandline -opc)[2] tracks"           -l "raw"             -d "Extract the data to a raw file"
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3; and contains -- (commandline -opc)[2] tracks"           -l "fullraw"         -d "Extract the data to a raw file including the CodecPrivate as header"
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3; and contains -- (commandline -opc)[2] chapters"    -s s -l "simple"          -d "Exports the chapter information in a simple format"
complete -f -c mkvextract -n "test (count (commandline -opc)) -ge 3; and contains -- (commandline -opc)[2] chapters" -r      -l "simple-language" -d "Uses the chapter names of the specified language"
