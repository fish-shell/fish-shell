complete -c unzip -s p -d "extract files to pipe, no messages"
complete -c unzip -s f -d "freshen existing files, create none"
complete -c unzip -s u -d "update files, create if necessary"
complete -c unzip -s v -d "list verbosely/show version info"
complete -c unzip -s x -d "exclude files that follow (in xlist)"
complete -c unzip -s l -d "list files (short format)"
complete -c unzip -s t -d "test compressed archive datamodifiers:"
complete -c unzip -s z -d "display archive comment only"
complete -c unzip -s T -d "timestamp archive to latest"
complete -c unzip -s d -d "extract files into dir" -xa '(__fish_complete_directories)'
complete -c unzip -s n -d "never overwrite existing files"
complete -c unzip -s o -d "overwrite files WITHOUT prompting"
complete -c unzip -s q -d "quiet mode"
complete -c unzip -o qq -d "quieter mode"
complete -c unzip -s a -d "auto-convert any text files"
complete -c unzip -s U -d "use escapes for all non-ASCII Unicode"
complete -c unzip -o UU -d "ignore any Unicode fields"
complete -c unzip -s j -d "junk paths (do not make directories)"
complete -c unzip -s aa -d "treat ALL files as text"
complete -c unzip -s C -d "match filenames case-insensitively"
complete -c unzip -s L -d "make (some) names lowercase"
complete -c unzip -s X -d "restore UID/GID info"
complete -c unzip -s V -d "retain VMS version numbers"
complete -c unzip -s K -d "keep setuid/setgid/tacky permissions"
complete -c unzip -s M -d "pipe through `more` pager"
# Some distros have -O and -I, some don't.
# Even "-h" might not be available.
if unzip -h 2>/dev/null | string match -rq -- -O
    complete -c unzip -s O -d "specify a character encoding for DOS, Windows and OS/2 archives" -x -a "(__fish_print_encodings)"
end
if unzip -h 2>/dev/null | string match -rq -- -I
    complete -c unzip -s I -d "specify a character encoding for UNIX and other archives" -x -a "(__fish_print_encodings)"
end

# Debian version of unzip
if unzip -v 2>/dev/null | string match -eq Debian

    # the first non-switch argument should be the zipfile
    complete -c unzip -n "__fish_is_nth_token 1" -k -xa '(__fish_complete_suffix .zip .jar .aar)'

    # Files thereafter are either files to include or exclude from the operation
    complete -c unzip -n 'not __fish_is_nth_token 1' -xa '(unzip -l (__fish_first_token) 2>/dev/null | string replace -r --filter ".*:\S+\s+(.*)" "\$1")'

else

    # all tokens should be zip files
    complete -c unzip -k -xa '(__fish_complete_suffix .zip .jar .aar)'

end
