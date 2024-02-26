function __fish_complete_tar -d "Peek inside of archives and list all files"
    set -l args (commandline -pxc)
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

## Operation mode
complete -c tar -s A -l catenate -l concatenate -d "Append archive to archive"
complete -c tar -s c -l create -d "Create archive"
complete -c tar -s d -l diff -l compare -d "Compare archive and filesystem"
complete -c tar -l delete -d "Delete from archive"
complete -c tar -s r -l append -d "Append files to archive"
complete -c tar -s t -l list -d "List archive"
complete -c tar -l test-label -d "Test the archive volume label"
complete -c tar -s u -l update -d "Append new files"
complete -c tar -s x -l extract -l get -d "Extract from archive"
complete -c tar -l show-defaults -d "Show built-in defaults for various tar options"
complete -c tar -s \? -l help -d "Display short option summary"
complete -c tar -l usage -d "List available options"
complete -c tar -l version -d "Print program version and copyright information"

## Operation modifiers

complete -c tar -l check-device -d "Check device numbers when creating incremental archives"
complete -c tar -s g -l listed-incremental -r -d "Handle new GNU-format incremental backups"
complete -c tar -l hole-detection -x -a "seek raw" -d "Use  METHOD  to detect holes in sparse files"
complete -c tar -s G -l incremental -d "Use old incremental GNU format"
complete -c tar -l ignore-failed-read -d "Do not exit with nonzero on unreadable files"
complete -c tar -l level -x -a 0 -d "Set  dump  level  for  a created listed-incremental archive"
complete -c tar -s n -l seek -d "Assume  the  archive is seekable"
complete -c tar -l no-check-device -d "Do not check device numbers when creating incremental archives"
complete -c tar -l no-seek -d "Assume the archive is not seekable"
complete -c tar -l occurrence -r -d "Process only the Nth occurrence of each file in the archive"
complete -c tar -l restrict -d "Disable the use of some potentially harmful options"
complete -c tar -l sparse-version -x -a "0.0 0.1 1.0" -d "Set which version  of  the  sparse  format  to  use"
complete -c tar -s S -l sparse -d "Handle sparse files"

## Overwrite control
complete -c tar -s k -l keep-old-files -d "Don't overwrite"
complete -c tar -l keep-newer-files -d "Don't replace existing files that are newer"
complete -c tar -l keep-directory-symlink -d "Don't replace existing symlinks"
complete -c tar -l no-overwrite-dir -d "Preserve metadata of existing directories"
complete -c tar -l one-top-level -d "Extract into directory"
complete -c tar -l overwrite -d "Overwrite existing files when extracting"
complete -c tar -l overwrite-dir -d "Overwrite metadata of existing directories"
complete -c tar -l recursive-unlink -d "Recursively remove all files in the directory prior to extracting it"
complete -c tar -l remove-files -d "Remove files after adding to archive"
complete -c tar -l skip-old-files -d "Don't replace existing files when extracting"
complete -c tar -s U -l unlink-first -d "Remove each file prior to extracting over it"
complete -c tar -s W -l verify -d "Verify archive"

## Output stream selection

complete -c tar -l ignore-command-error -d "Ignore subprocess exit codes"
complete -c tar -l no-ignore-command-error -d "Treat non-zero exit codes of children as error"
complete -c tar -s O -l to-stdout -d "Extract to stdout"
complete -c tar -l to-command -r -d "Pipe  extracted files to COMMAND"

## Handling of file attributes

complete -c tar -l atime-preserve -d "Keep access time"
complete -c tar -l delay-directory-restore -d "Delay  setting  modification  times"
complete -c tar -l group -x -d "Force  NAME  as  group for added files"
complete -c tar -l group-map -r -d "Read group translation map from FILE"
complete -c tar -l mode -x -d "Force symbolic mode CHANGES for added files"
complete -c tar -l mtime -r -d "Set mtime for added files"
complete -c tar -s m -l touch -d "Don't extract modification time"
complete -c tar -l no-delay-directory-restore -d "Cancel the effect of the prior --delay-directory-restore"
complete -c tar -l no-same-owner -d "Extract files as yourself"
complete -c tar -l no-same-permissions -d "Apply the user's umask when extracting"
complete -c tar -l numeric-owner -d "Always use numbers for user/group names"
complete -c tar -l owner -x -d "Force NAME as owner for added files"
complete -c tar -l owner-map -r -d "Read owner translation map from FILE"
complete -c tar -s p -l same-permissions -l preserve-permissions -d "Extract all permissions"
complete -c tar -l same-owner -d "Try  extracting  files with the same ownership"
complete -c tar -s s -l same-order -l preserve-order -d "Do not sort file arguments"
complete -c tar -l sort -x -a "none name inode" -d "sort directory entries according to ORDER"

## Extended file attributes

complete -c tar -l acls -d "Enable POSIX ACLs support"
complete -c tar -l no-acls -d "Disable POSIX ACLs support"
complete -c tar -l selinux -d "Enable SELinux context support"
complete -c tar -l no-selinux -d "Disable SELinux context support"
complete -c tar -l xattrs -d "Enable extended attributes support"
complete -c tar -l no-xattrs -d "Disable extended attributes support"
complete -c tar -l xattrs-exclude -d "Specify the exclude pattern for xattr keys"
complete -c tar -l xattrs-include -d "Specify the include pattern for xattr keys"

## Device selection and switching
complete -c tar -s f -l file -r -d "Archive file"
complete -c tar -l force-local -r -d "Archive file is local even if it has a colon"
complete -c tar -s F -l info-script -l new-volume-script -r -d "Run script at end of tape"
complete -c tar -s L -l tape-length -r -d "Tape length"
complete -c tar -s M -l multi-volume -d "Multi volume archive"
complete -c tar -l rmt-command -r -d "Use COMMAND instead of rmt"
complete -c tar -l rsh-command -r -d "Use COMMAND instead of rsh"
complete -c tar -l volno-file -r -d "keep track of which volume of a multi-volume archive it is working in FILE"

complete -c tar -s f -l file -r -d "Archive file"
complete -c tar -s f -l file -r -d "Archive file"
complete -c tar -s f -l file -r -d "Archive file"

complete -c tar -s b -l block-size -d "Block size"
complete -c tar -s B -l read-full-blocks -d "Reblock while reading"
complete -c tar -s C -l directory -r -d "Change directory"
complete -c tar -l checkpoint -d "Print directory names"
complete -c tar -l force-local -d "Archive is local"
complete -c tar -s h -l dereference -d "Dereference symlinks"
complete -c tar -s i -l ignore-zeros -d "Ignore zero block in archive"
complete -c tar -s K -l starting-file -r -d "Starting file in archive"
complete -c tar -s l -l one-file-system -d "Stay in local filesystem"
complete -c tar -s m -l modification-time -d "Don't extract modification time"
complete -c tar -s N -l after-date -r -d "Only store newer files"
complete -c tar -l newer -r -d "Only store newer files"
complete -c tar -s o -l old-archive -d "Use V7 format"
complete -c tar -l portability -d "Use V7 format"
complete -c tar -s P -l absolute-paths -d "Don't strip leading /"
complete -c tar -l preserve -d "Preserve all permissions and do not sort file arguments"
complete -c tar -s R -l record-number -d "Show record number"
complete -c tar -s T -l files-from -r -d "Extract file from file"
complete -c tar -l null -d "-T has null-terminated names"
complete -c tar -l totals -d "Print total bytes written"
complete -c tar -s v -l verbose -d "Verbose mode"
complete -c tar -s V -l label -r -d "Set volume name"
complete -c tar -s w -l interactive -d "Ask for confirmation"
complete -c tar -l confirmation -d "Ask for confirmation"
complete -c tar -l exclude -r -d "Exclude file"
complete -c tar -s X -l exclude-from -r -d "Exclude files listed in specified file"

## Compression options
complete -c tar -s a -l auto-compress -d "Use archive suffix to determine the compression program"
complete -c tar -s I -l use-compress-program -r -d "Filter through specified program"
complete -c tar -s j -l bzip2 -d "Filter through bzip2"
complete -c tar -s J -l xz -d "Filter through xz"
complete -c tar -l lzip -d "Filter through lzip"
complete -c tar -l lzma -d "Filter through lzma"
complete -c tar -l lzop -d "Filter through lzop"
complete -c tar -l no-auto-compress -d "Do not use archive suffix to determine the compression program"
complete -c tar -s z -l gzip -l gunzip -l ungzip -d "Filter through gzip"
complete -c tar -s Z -l compress -l uncompress -d "Filter through compress"
complete -c tar -l zstd -d "Filter through zstd"
