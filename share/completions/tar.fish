function __fish_complete_tar -d "Peek inside of archives and list all files"
    set -l args (commandline -poc)
    while count $args >/dev/null
        switch $args[1]
            case '-*f' --file
                set -e args[1]
                if test -f $args[1]
                    set -l file_list (tar -atf $args[1] 2> /dev/null)
                    if test -n "$file_list"
                        printf "%s\tArchived file\n" $file_list
                    end
                    return
                end
            case '*'
                set -e args[1]
                continue
        end
    end
end

complete -c tar -a "(__fish_complete_tar)"

complete -c tar -s A -l catenate -d "Append archive to archive"
complete -c tar -l concatenate -d "Append archive to archive"
complete -c tar -s c -l create -d "Create archive"
complete -c tar -s d -l diff -d "Compare archive and filesystem"
complete -c tar -l compare -d "Compare archive and filesystem"
complete -c tar -l delete -d "Delete from archive"
complete -c tar -s r -l append -d "Append files to archive"
complete -c tar -s t -l list -d "List archive"
complete -c tar -s u -l update -d "Append new files"
complete -c tar -s x -l extract -d "Extract from archive"
complete -c tar -l get -d "Extract from archive"
complete -c tar -l atime-preserve -d "Keep access time"
complete -c tar -s b -l block-size -d "Block size"
complete -c tar -s B -l read-full-blocks -d "Reblock while reading"
complete -c tar -s C -l directory -r -d "Change directory"
complete -c tar -l checkpoint -d "Print directory names"
complete -c tar -s f -l file -r -d "Archive file"
complete -c tar -l force-local -d "Archive is local"
complete -c tar -s F -l info-script -d "Run script at end of tape"
complete -c tar -s G -l incremental -d "Use old incremental GNU format"
complete -c tar -s g -l listed-incremental -d "Use new incremental GNU format"
complete -c tar -s h -l dereference -d "Dereference symlinks"
complete -c tar -s i -l ignore-zeros -d "Ignore zero block in archive"
complete -c tar -s j -l bzip2 -d "Filter through bzip2"
complete -c tar -l ignore-failed-read -d "Don't exit on unreadable files"
complete -c tar -s k -l keep-old-files -d "Don't overwrite"
complete -c tar -s K -l starting-file -r -d "Starting file in archive"
complete -c tar -s l -l one-file-system -d "Stay in local filesystem"
complete -c tar -s L -l tape-length -r -d "Tape length"
complete -c tar -s m -l modification-time -d "Don't extract modification time"
complete -c tar -l touch -d "Don't extract modification time"
complete -c tar -s M -l multi-volume -d "Multi volume archive"
complete -c tar -s N -l after-date -r -d "Only store newer files"
complete -c tar -l newer -r -d "Only store newer files"
complete -c tar -s o -l old-archive -d "Use V7 format"
complete -c tar -l portability -d "Use V7 format"
complete -c tar -s O -l to-stdout -d "Extract to stdout"
complete -c tar -s p -l same-permissions -d "Extract all permissions"
complete -c tar -l preserve-permissions -d "Extract all permissions"
complete -c tar -s P -l absolute-paths -d "Don't strip leading /"
complete -c tar -l preserve -d "Preserve all permissions and do not sort file arguments"
complete -c tar -s R -l record-number -d "Show record number"
complete -c tar -l remove-files -d "Remove files after adding to archive"
complete -c tar -s s -l same-order -d "Do not sort file arguments"
complete -c tar -l preserve-order -d "Do not sort file arguments"
complete -c tar -l same-owner -d "Preserve file ownership"
complete -c tar -s S -l sparse -d "Handle sparse files"
complete -c tar -s T -l files-from -r -d "Extract file from file"
complete -c tar -l null -d "-T has null-terminated names"
complete -c tar -l totals -d "Print total bytes written"
complete -c tar -s v -l verbose -d "Verbose mode"
complete -c tar -s V -l label -r -d "Set volume name"
complete -c tar -l version -d "Display version and exit"
complete -c tar -s w -l interactive -d "Ask for confirmation"
complete -c tar -l confirmation -d "Ask for confirmation"
complete -c tar -s W -l verify -d "Verify archive"
complete -c tar -l exclude -r -d "Exclude file"
complete -c tar -s X -l exclude-from -r -d "Exclude files listed in specified file"
complete -c tar -s Z -l compress -d "Filter through compress"
complete -c tar -l uncompress -d "Filter through compress"
complete -c tar -s z -l gzip -d "Filter through gzip"
complete -c tar -l gunzip -d "Filter through gzip"
complete -c tar -l use-compress-program -r -d "Filter through specified program"
complete -c tar -s J -l xz -d "Filter through xz"
complete -c tar -l lzip -d "Filter through lzip"
complete -c tar -l lzma -d "Filter through lzma"
complete -c tar -l lzop -d "Filter through lzop"
complete -c tar -l zstd -d "Filter through zstd"
