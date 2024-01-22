# Sample output of 'mkvmerge -i file.mkv'
#
# File 'file.mkv': container: Matroska
# Track ID 0: video (MPEG-4p10/AVC/h.264)
# Track ID 1: audio (AAC)
# Track ID 2: subtitles (SubStationAlpha)
# Attachment ID 1: type 'application/x-truetype-font', size 53532 bytes, file name 'some_font.ttf'
# Chapters: 7 entries

function __fish_mkvextract_get_mode
    set -l cmd (commandline -xpc)
    set -l skip_next 0
    for c in $cmd[2..-1]
        test $skip_next = 1; and set skip_next 0; and continue
        switch $c
            # All extraction modes
            case tracks tags attachments chapters cuesheet timecodes_v2 cues
                echo $c
                return 0
                # General flags that require an option - skip next argument
            case --ui-language --command-line-charset --output-charset -r --redirect-output --ui-language --debug --engage
                set skip_next 1
                # If any of these flags are present, all other args are ignored
            case -h --help -V --version --check-for-updates
                return 1
        end
    end
    return 1
end

function __fish_mkvextract_get_file
    set -l cmd (commandline -xpc)
    # Any invocation with a file specified will already have >= 2 args
    if not set -q cmd[3]
        return 1
    end
    if not set -l mode (__fish_mkvextract_get_mode)
        return 1
    end
    set -l skip_next 0
    # Files strictly appear after the mode specifier
    set -l mode_found 0
    for c in $cmd[2..-1]
        test $skip_next = 1; and set skip_next 0; and continue
        switch $c
            # We've found the mode specifier, now look for the file
            case $mode
                set mode_found 1
                # Track-specific flags can appear between mode specifier and file
            case --ui-language --command-line-charset --output-charset -r --redirect-output --ui-language --debug --engage -c --blockadd --simple-language
                set skip_next 1
                # These flags now need to be explicitly skipped
            case -f --parse-fully -v --verbose -q --quiet '@*' --cuesheet --raw --fullraw -s --simple --gui-mode
                continue
            case -h --help -V --version --check-for-updates
                return 1
            case '*'
                if test $mode_found = 1
                    echo -- $c
                    return 0
                end
        end
    end
    return 1
end

function __fish_mkvextract_using_mode
    set -l mode (__fish_mkvextract_get_mode)
    and set -l file (__fish_mkvextract_get_file)
    and contains -- $mode $argv
end

function __fish_mkvextract_print_attachments
    if set -l matroska (__fish_mkvextract_get_file)
        if set -l info (mkvmerge -i $matroska)
            string match 'Attachment ID*' -- $info | string replace -r '.*?(\d+).*? type \'(.*?)\'.*?file name \'(.*?)\'' '$1:\t$3 ($2)'
        end
    end
end

function __fish_mkvextract_print_tracks
    if set -l matroska (__fish_mkvextract_get_file)
        if set -l info (mkvmerge -i $matroska)
            string match 'Track ID*' -- $info | string replace -r '.*?(\d+): (.*)' '$1:\t$2'
        end
    end
end

# simple options
complete -f -c mkvextract -s V -l version -d 'Show version information'
complete -f -c mkvextract -s h -l help -d 'Show help'
complete -f -c mkvextract -l check-for-updates -d 'Check online for updates'
# extraction modes
complete -f -c mkvextract -n 'not __fish_mkvextract_get_mode' -a tracks -d 'Extract tracks to external files'
complete -f -c mkvextract -n 'not __fish_mkvextract_get_mode' -a tags -d 'Extract tags as XML'
complete -f -c mkvextract -n 'not __fish_mkvextract_get_mode' -a attachments -d 'Extract attachments'
complete -f -c mkvextract -n 'not __fish_mkvextract_get_mode' -a chapters -d 'Extract chapters as XML'
complete -f -c mkvextract -n 'not __fish_mkvextract_get_mode' -a cuesheet -d 'Extract chapters and tags as CUE sheet'
complete -f -c mkvextract -n 'not __fish_mkvextract_get_mode' -a timecodes_v2 -d 'Extract timecodes of a track as timecode v2 file'
complete -f -c mkvextract -n 'not __fish_mkvextract_get_mode' -a cues -d 'Extract cue information as text file'
# tracks/attachments when appropriate
complete -f -c mkvextract -n '__fish_mkvextract_using_mode tracks timecodes_v2 cues' -a '(__fish_mkvextract_print_tracks)'
complete -f -c mkvextract -n '__fish_mkvextract_using_mode attachments' -a '(__fish_mkvextract_print_attachments)'
# these extraction modes have no parameters other than optional flags and the file
complete -f -c mkvextract -n '__fish_mkvextract_using_mode tags chapters cuesheet'
# general flags
complete -c mkvextract -s f -l parse-fully -d 'Parse the whole file instead of relying on the index'
complete -c mkvextract -s v -l verbose -d 'Increase verbosity'
complete -c mkvextract -s q -l quiet -d 'Suppress status output'
complete -c mkvextract -r -l ui-language -d 'Force a locale'
complete -c mkvextract -r -l command-line-charset -d 'Charset for strings on the command line'
complete -c mkvextract -r -l output-charset -d 'Outputs messages in specified charset'
complete -c mkvextract -r -s r -l redirect-output -d 'Redirect all messages into a file'
complete -c mkvextract -r -l debug -d 'Turn on debugging for a specific feature'
complete -c mkvextract -r -l engage -d 'Turn on an experimental feature'
complete -c mkvextract -l gui-mode -d 'Enable GUI mode'
# mode-specific flags
complete -f -c mkvextract -n '__fish_mkvextract_using_mode tracks' -r -s c -d 'Convert text subtitles to a charset'
complete -f -c mkvextract -n '__fish_mkvextract_using_mode tracks' -l cuesheet -d 'Also try to extract the CUE sheet'
complete -f -c mkvextract -n '__fish_mkvextract_using_mode tracks' -r -l blockadd -d 'Keep only the BlockAdditions up to the specified level'
complete -f -c mkvextract -n '__fish_mkvextract_using_mode tracks' -l raw -d 'Extract the data to a raw file'
complete -f -c mkvextract -n '__fish_mkvextract_using_mode tracks' -l fullraw -d 'Extract the data to a raw file including the CodecPrivate as header'
complete -f -c mkvextract -n '__fish_mkvextract_using_mode chapters' -s s -l simple -d 'Exports the chapter information in a simple format'
complete -f -c mkvextract -n '__fish_mkvextract_using_mode chapters' -r -l simple-language -d 'Uses the chapter names of the specified language'
