# Completions for gio (a part of GLib)

# Scheme name (see: https://wiki.gnome.org/Projects/gvfs/schemes)
set -l supported_schemes admin afc afp archive burn cdda computer dav dav+sd davs davs+sd dns-sd file ftp ftpis ftps google-drive gphoto2 http https localtest mtp network nfs recent sftp smb ssh test trash

for scheme in $supported_schemes
    complete -c gio -n "__fish_seen_subcommand_from cat copy info list mkdir monitor mount move open rename remove save set trash tree" -a "$scheme": -d Scheme
end

# Commands
complete -f -c gio -n __fish_use_subcommand -a help -d "Print help"
complete -f -c gio -n __fish_use_subcommand -a version -d "Print version"
complete -f -c gio -n __fish_use_subcommand -a cat -d "Concatenate files to stdout"
complete -f -c gio -n __fish_use_subcommand -a copy -d "Copy files"
complete -f -c gio -n __fish_use_subcommand -a info -d "Show information about locations"
complete -f -c gio -n __fish_use_subcommand -a list -d "List the contents of locations"
complete -f -c gio -n __fish_use_subcommand -a mime -d "Get or set the handler"
complete -f -c gio -n __fish_use_subcommand -a mkdir -d "Create directories"
complete -f -c gio -n __fish_use_subcommand -a monitor -d "Monitor files"
complete -f -c gio -n __fish_use_subcommand -a mount -d "Mount or unmount the locations"
complete -f -c gio -n __fish_use_subcommand -a move -d "Move files"
complete -f -c gio -n __fish_use_subcommand -a open -d "Open files"
complete -f -c gio -n __fish_use_subcommand -a rename -d "Rename a file"
complete -f -c gio -n __fish_use_subcommand -a remove -d "Delete files"
complete -f -c gio -n __fish_use_subcommand -a save -d "Read from stdin and save"
complete -f -c gio -n __fish_use_subcommand -a set -d "Set a file attribute"
complete -f -c gio -n __fish_use_subcommand -a trash -d "Move files to the trash"
complete -f -c gio -n __fish_use_subcommand -a tree -d "Lists the contents of locations in a tree"

# Arguments of help command
complete -f -c gio -n "__fish_seen_subcommand_from help" -a "version cat copy info list mime mkdir monitor mount move open rename remove save set trash tree" -d Command

# Arguments of mime command
function __fish_gio_list_mimetypes
    set -l python (__fish_anypython) || return
    $python -S -c 'import mimetypes; mimetypes.inited or mimetypes.init(); print("\n".join(sorted(set(mimetypes.types_map.values()))))'
end

complete -f -c gio -n "__fish_seen_subcommand_from mime" -a "(__fish_gio_list_mimetypes)" -d "MIME type"

# Common options
complete -f -c gio -n "__fish_seen_subcommand_from copy move" -s T -l no-target-directory -d "No target directory"
complete -f -c gio -n "__fish_seen_subcommand_from copy move" -s p -l progress -d "Show progress"
complete -f -c gio -n "__fish_seen_subcommand_from copy move" -s i -l interactive -d "Prompt before overwrite"
complete -f -c gio -n "__fish_seen_subcommand_from copy move save" -s b -l backup -d "Backup existing destination files"
complete -x -c gio -n "__fish_seen_subcommand_from info list" -s a -l attributes -d "The attributes to get"
complete -f -c gio -n "__fish_seen_subcommand_from info list set" -s n -l nofollow-symlinks -d "Don't follow symbolic links"
complete -f -c gio -n "__fish_seen_subcommand_from list tree" -s h -l hidden -d "Show hidden files"
complete -f -c gio -n "__fish_seen_subcommand_from remove trash" -s f -l force -d "Ignore nonexistent files"

# Options of copy command
complete -f -c gio -n "__fish_seen_subcommand_from copy" -l preserve -d "Preserve all attributes"
complete -f -c gio -n "__fish_seen_subcommand_from copy" -s P -l no-dereference -d "Never follow symbolic links"
complete -f -c gio -n "__fish_seen_subcommand_from copy" -l default-permissions -d "Use default permissions"

# Options of info command
complete -f -c gio -n "__fish_seen_subcommand_from info" -s w -l query-writable -d "List writable attributes"
complete -f -c gio -n "__fish_seen_subcommand_from info" -s f -l filesystem -d "Get file system info"

# Options of list command
complete -f -c gio -n "__fish_seen_subcommand_from list" -s l -l long -d "Use a long listing format"
complete -f -c gio -n "__fish_seen_subcommand_from list" -s d -l print-display-names -d "Print display names"
complete -f -c gio -n "__fish_seen_subcommand_from list" -s u -l print-uris -d "Print full URIs"

# Options of mkdir command
complete -f -c gio -n "__fish_seen_subcommand_from mkdir" -s p -l parent -d "Create parent directories"

# Options of monitor command
complete -r -c gio -n "__fish_seen_subcommand_from monitor" -s d -l dir -d "Monitor a directory"
complete -r -c gio -n "__fish_seen_subcommand_from monitor" -s f -l file -d "Monitor a file"
complete -r -c gio -n "__fish_seen_subcommand_from monitor" -s D -l direct -d "Monitor a file directly"
complete -r -c gio -n "__fish_seen_subcommand_from monitor" -s s -l silent -d "Monitors a file directly without reporting changes"
complete -f -c gio -n "__fish_seen_subcommand_from monitor" -s n -l no-moves -d "Don't report move events"
complete -f -c gio -n "__fish_seen_subcommand_from monitor" -s m -l mounts -d "Watch for mount events"

# Options of mount command
complete -f -c gio -n "__fish_seen_subcommand_from mount" -s m -l mountable -d "Mount as mountable"
complete -x -c gio -n "__fish_seen_subcommand_from mount" -s d -l device -d "Mount volume"
complete -f -c gio -n "__fish_seen_subcommand_from mount" -s u -l unmount -d Unmount
complete -f -c gio -n "__fish_seen_subcommand_from mount" -s e -l eject -d Eject
complete -x -c gio -n "__fish_seen_subcommand_from mount" -s t -l stop -d "Stop drive"
complete -x -c gio -n "__fish_seen_subcommand_from mount" -s s -l unmount-scheme -a "$supported_schemes" -d "Unmount all mounts with the given scheme"
complete -f -c gio -n "__fish_seen_subcommand_from mount" -s f -l force -d "Ignore outstanding file operations"
complete -f -c gio -n "__fish_seen_subcommand_from mount" -s a -l anonymous -d "Use an anonymous user"
complete -f -c gio -n "__fish_seen_subcommand_from mount" -s l -l list -d List
complete -f -c gio -n "__fish_seen_subcommand_from mount" -s o -l monitor -d "Monitor events"
complete -f -c gio -n "__fish_seen_subcommand_from mount" -s i -l detail -d "Show extra information"
complete -x -c gio -n "__fish_seen_subcommand_from mount" -l tcrypt-pim -d "The numeric PIM when unlocking a VeraCrypt volume"
complete -f -c gio -n "__fish_seen_subcommand_from mount" -l tcrypt-hidden -d "Mount a TCRYPT hidden volume"
complete -f -c gio -n "__fish_seen_subcommand_from mount" -l tcrypt-system -d "Mount a TCRYPT system volume"

# Options of move command
complete -f -c gio -n "__fish_seen_subcommand_from move" -s C -l no-copy-fallback -d "Don't use copy and delete fallback"

# Options of save command
complete -f -c gio -n "__fish_seen_subcommand_from save" -s c -l create -d "Only create if not existing"
complete -f -c gio -n "__fish_seen_subcommand_from save" -s a -l append -d "Append to end of file"
complete -f -c gio -n "__fish_seen_subcommand_from save" -s p -l private -d "Restrict access to the current user"
complete -f -c gio -n "__fish_seen_subcommand_from save" -s u -l unlink -d "Replace as if the destination didn't exist"
complete -f -c gio -n "__fish_seen_subcommand_from save" -s v -l print-etag -d "Print new etag at end"
complete -x -c gio -n "__fish_seen_subcommand_from save" -s e -l etag -d "The etag of the file being overwritten"

# Options of set command
complete -x -c gio -n "__fish_seen_subcommand_from set" -s t -l type -d "Type of the attribute"

# Options of trash command
complete -f -c gio -n "__fish_seen_subcommand_from trash" -l empty -d "Empty the trash"

# Options of tree command
complete -f -c gio -n "__fish_seen_subcommand_from tree" -s l -l follow-symlinks -d "Follow symbolic links"
