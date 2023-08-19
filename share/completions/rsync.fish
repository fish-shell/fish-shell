function __rsync_remote_target
    commandline -ct | string match -r '.*::?(?:.*/)?' | string unescape
end

function __rsync_parse_flags -d "Print info|help FLAGS from help output"
    set -l helptext $argv[2..-1] # Skips header line
    for line in $helptext
        set -l tokens (string match -r "([A-Z]+)(?: {2,})(.+)" $line)
        if test (count $tokens) -ge 3
            echo $tokens[2]\t$tokens[3]
        end
    end
end

complete -c rsync -s v -l verbose -d "Increase verbosity"
complete -c rsync -l info -d "Fine-grained informational verbosity" -xa "
(__rsync_parse_flags (rsync --info=help))"
complete -c rsync -l debug -d "Fine-grained debug verbosity" -xa "
(__rsync_parse_flags (rsync --debug=help))"
complete -c rsync -l stderr -xa 'errors all client' -d "change stderr output mode, default: errors"
complete -c rsync -s q -l quiet -d "Suppress non-error messages"
complete -c rsync -l no-motd -d "Suppress daemon-mode MOTD"
complete -c rsync -s c -l checksum -d "Skip based on checksum, not mod-time & size"
complete -c rsync -s a -l archive -d "Archive mode; same as -rlptgoD (no -H)"
complete -c rsync -s r -l recursive -d "Recurse into directories"
complete -c rsync -s R -l relative -d "Use relative path names"
complete -c rsync -l no-implied-dirs -d "Don’t send implied dirs with --relative"
complete -c rsync -s b -l backup -d "Make backups (see --suffix & --backup-dir)"
complete -c rsync -l backup-dir -xa '(__fish_complete_directories)' -d "Make backups into hierarchy based in DIR"
complete -c rsync -l suffix -d "Backup suffix (default ~ w/o --backup-dir)"
complete -c rsync -s u -l update -d "Skip files that are newer on the receiver"
complete -c rsync -l inplace -d "Update destination files in-place"
complete -c rsync -l append -d "Append data onto shorter files without verifing old content"
complete -c rsync -l append-verify -d "Append with full file checksum, including old data"
complete -c rsync -s d -l dirs -d "Transfer directories without recursing"
complete -c rsync -l mkpath -d "Create the destination's path component"
complete -c rsync -s l -l links -d "Copy symlinks as symlinks"
complete -c rsync -s L -l copy-links -d "Transform symlink into referent file/dir"
complete -c rsync -l copy-unsafe-links -d "Only \"unsafe\" symlinks are transformed"
complete -c rsync -l safe-links -d "Ignore symlinks that point outside the tree"
complete -c rsync -l munge-links -d "Munge symlinks to make them safe & unusable"
complete -c rsync -s k -l copy-dirlinks -d "Transform symlink to dir into referent dir"
complete -c rsync -s K -l keep-dirlinks -d "Treat symlinked dir on receiver as dir"
complete -c rsync -s H -l hard-links -d "Preserve hard links"
complete -c rsync -s p -l perms -d "Preserve permissions"
complete -c rsync -s E -l executability -d "Preserve executability"
complete -c rsync -l chmod -d "Change destination permissions"
complete -c rsync -s A -l acls -d "Preserve ACLs (implies -p) [non-standard]"
complete -c rsync -s X -l xattrs -d "Preserve extended attrs (implies -p) [n.s.]"
complete -c rsync -s o -l owner -d "Preserve owner (super-user only)"
complete -c rsync -s g -l group -d "Preserve group"
complete -c rsync -l devices -d "Preserve device files (super-user only)"
complete -c rsync -l specials -d "Preserve special files"
complete -c rsync -s D -d "Same as --devices --specials"
complete -c rsync -s t -l times -d "Preserve modification times"
complete -c rsync -s U -l atimes -d "Preserve access (use) times"
complete -c rsync -l open-noatime -d "Avoid changing the atime on opened files"
complete -c rsync -l crtimes -d "Preserve creation (birth) times"
complete -c rsync -s O -l omit-dir-times -d "Omit directories when preserving times"
complete -c rsync -s J -l omit-link-times -d "Omit symlinks when preserving times"
complete -c rsync -l super -d "Receiver attempts super-user activities"
complete -c rsync -l fake-super -d "Store/recover privileged attrs using xattrs"
complete -c rsync -s S -l sparse -d "Handle sparse files efficiently"
complete -c rsync -l preallocate -d "Allocate dest files before writing them"
complete -c rsync -l write-devices -d "Write to devices as files (implies --inplace)"
complete -c rsync -s n -l dry-run -d "Show what would have been transferred"
complete -c rsync -s W -l whole-file -d "Copy files whole (without rsync algorithm)"
complete -c rsync -l cc -l checksum-choice -xa 'xxh128 xxh3 xxh64 md5 md4 none' -d "Choose the checksum algorithm"
complete -c rsync -s x -l one-file-system -d "Don’t cross filesystem boundaries"
complete -c rsync -s B -l block-size -x -d "Force a fixed checksum block-size"
complete -c rsync -s e -l rsh -x -d "Specify the remote shell to use"
complete -c rsync -l rsync-path -x -d "Specify the rsync to run on remote machine"
complete -c rsync -l existing -d "Ignore non-existing files on receiving side"
complete -c rsync -l ignore-existing -d "Ignore files that already exist on receiver"
complete -c rsync -l remove-sent-files -d "Sent files/symlinks are removed from sender"
complete -c rsync -l remove-source-files -d "Remove all synced files from source/sender"
complete -c rsync -l del -d "An alias for --delete-during"
complete -c rsync -l delete -d "Delete files that don’t exist on sender"
complete -c rsync -l delete-before -d "Receiver deletes before transfer (default)"
complete -c rsync -l delete-during -d "Receiver deletes during xfer, not before"
complete -c rsync -l delete-delay -d "Find deletions during, delete after"
complete -c rsync -l delete-after -d "Receiver deletes after transfer, not before"
complete -c rsync -l delete-excluded -d "Also delete excluded files on receiver"
complete -c rsync -l ignore-missing-args -d "Ignore missing source args without error"
complete -c rsync -l delete-missing-args -d "Delete missing source args from destination"
complete -c rsync -l ignore-errors -d "Delete even if there are I/O errors"
complete -c rsync -l force -d "Force deletion of dirs even if not empty"
complete -c rsync -l max-delete -xa '(seq 0 10)' -d "Don’t delete more than NUM files"
complete -c rsync -l max-size -xa '(seq 0 10){K,M,G}' -d "Don’t transfer any file larger than SIZE"
complete -c rsync -l min-size -xa '(seq 0 10){K,M,G}' -d "Don’t transfer any file smaller than SIZE"
complete -c rsync -l max-alloc -xa '(seq 0 10){K,M,G}' -d "Change process memory allocation limit"
complete -c rsync -l partial -d "Keep partially transferred files"
complete -c rsync -l partial-dir -xa '(__fish_complete_directories)' -d "Put a partially transferred file into DIR"
complete -c rsync -l delay-updates -d "Put all updated files into place at end"
complete -c rsync -s m -l prune-empty-dirs -d "Prune empty directory chains from file-list"
complete -c rsync -l numeric-ids -d "Don’t map uid/gid values by user/group name"
complete -c rsync -l usermap -xa '(__fish_complete_users)' -d "Custom username mapping"
complete -c rsync -l groupmap -xa '(__fish_complete_groups)' -d "Custom username mapping"
complete -c rsync -l chown -xa '(__fish_complete_users;__fish_complete_groups)' -d "Combined username/groupname mapping"
complete -c rsync -l timeout -d "Set I/O timeout in seconds"
complete -c rsync -l cotimeout -d "Set daemon connection timeout in seconds"
complete -c rsync -s I -l ignore-times -d "Don’t skip files that match size and time"
complete -c rsync -l size-only -d "Skip files that match in size"
complete -c rsync -l modify-window -xa '(seq 0 10)' -d "Compare NUM mod-times with reduced accuracy"
complete -c rsync -s T -l temp-dir -xa '(__fish_complete_directories)' -d "Create temporary files in directory DIR"
complete -c rsync -s y -l fuzzy -d "Find similar file for basis if no dest file"
complete -c rsync -l compare-dest -xa '(__fish_complete_directories)' -d "Also compare received files relative to DIR"
complete -c rsync -l copy-dest -xa '(__fish_complete_directories)' -d "Like compare-dest but also copies unchanged files"
complete -c rsync -l link-dest -xa '(__fish_complete_directories)' -d "Hardlink to files in DIR when unchanged"
complete -c rsync -s z -l compress -d "Compress file data during the transfer"
complete -c rsync -l zc -l compress-choice -xa 'zstd lz4 zlibx zlib none' -d "Choose the compression algorithm"
complete -c rsync -l compress-level -x -d "Explicitly set compression level (aka --zl)"
complete -c rsync -l skip-compress -x -d "Skip compressing files with suffix in LIST"
complete -c rsync -s C -l cvs-exclude -d "Auto-ignore files in the same way CVS does"
complete -c rsync -s f -l filter -d "Add a file-filtering RULE"
complete -c rsync -s F -d "Same as --filter=’dir-merge /.rsync-filter’ repeated: --filter='- .rsync-filter'"
complete -c rsync -l exclude -d "Exclude files matching PATTERN"
complete -c rsync -l exclude-from -d "Read exclude patterns from FILE" -r
complete -c rsync -l include -d "Don’t exclude files matching PATTERN"
complete -c rsync -l include-from -r -d "Read include patterns from FILE"
complete -c rsync -l files-from -r -d "Read list of source-file names from FILE"
complete -c rsync -s 0 -l from0 -d "All *from/filter files are delimited by 0s"
complete -c rsync -s s -l protect-args -d "No space-splitting; wildcard chars only"
complete -c rsync -l copy-as -xa '(__fish_complete_users;__fish_complete_groups)' -d "Specify user & optional group for the copy"
complete -c rsync -l address -d "Bind address for outgoing socket to daemon"
complete -c rsync -l port -d "Specify double-colon alternate port number"
complete -c rsync -l sockopts -d "Specify custom TCP options"
complete -c rsync -l blocking-io -d "Use blocking I/O for the remote shell"
complete -c rsync -l outbuf -xa 'N L B' -d "Set out buffering to None, Line, or Block"
complete -c rsync -l stats -d "Give some file-transfer stats"
complete -c rsync -s 8 -l 8-bit-output -d "Leave high-bit chars unescaped in output"
complete -c rsync -s h -l human-readable -d "Output numbers in a human-readable format"
complete -c rsync -l progress -d "Show progress during transfer"
complete -c rsync -s P -d "Same as --partial --progress"
complete -c rsync -s i -l itemize-changes -d "Output a change-summary for all updates"
complete -c rsync -s M -l remote-option -d "Send OPTION to the remote side only"
complete -c rsync -l out-format -x -d "Output updates using the specified FORMAT"
complete -c rsync -l log-file -d "log what we're doing to the specified FILE"
complete -c rsync -l log-file-format -x -d "log updates using the specified FMT"
complete -c rsync -l password-file -r -d "Read password from FILE"
complete -c rsync -l early-input -d "Use FILE for daemon's early exec input"
complete -c rsync -l list-only -d "List the files instead of copying them"
complete -c rsync -l bwlimit -xa '(seq 0 10){K,M,G}' -d "Limit I/O bandwidth; optional unit (KB/s default)"
complete -c rsync -l stop-after -x -d "Stop rsync after MINS minutes have elapsed"
complete -c rsync -l stop-at -x -d "Stop rsync at the specified point in time"
complete -c rsync -l write-batch -r -d "Write a batched update to FILE"
complete -c rsync -l only-write-batch -d "Like --write-batch but w/o updating dest"
complete -c rsync -l read-batch -r -d "Read a batched update from FILE"
complete -c rsync -l protocol -x -d "Force an older protocol version to be used"
complete -c rsync -l iconv -x -d "Request charset conversion of filenames"
complete -c rsync -l checksum-seed -x -d "Set block/file checksum seed (advanced)"
complete -c rsync -s 4 -l ipv4 -d "Prefer IPv4"
complete -c rsync -s 6 -l ipv6 -d "Prefer IPv6"
complete -c rsync -l daemon -d "Run as an rsync daemon"
complete -c rsync -l config -r -d "Specify alternate rsyncd.conf file"
complete -c rsync -l dparam -x -d "Override global daemon config parameter"
complete -c rsync -l no-detach -x -d "Do not detach from the parent"
complete -c rsync -s V -l version -d "Display version and feature info"
complete -c rsync -s h -l help -d "Display help and exit"

#
# Hostname completion
#
complete -c rsync -d Hostname -a "
(__fish_print_hostnames):

(
	# Prepend any username specified in the completion to the hostname.
	commandline -ct |sed -ne 's/\(.*@\).*/\1/p'
)(__fish_print_hostnames):

(__fish_print_users)@\tUsername
"

#
# Remote path
#
# Unfortunately, the escaping changed in version 3.2.4.
# Even more unfortunate, rsync's version output is horrible:
# rsync  version v3.2.4  protocol version 31
# Copyright (C) 1996-2022 by Andrew Tridgell, Wayne Davison, and others.
# Web site: https://rsync.samba.org/
# ... (and then a ton of lines about capabilities)
#
# This includes multiple spaces, the version might start with "v" depending on whether it's
# built from a git tag or not...

set -l new_escaping # has an element if the new escaping style introduced in 3.2.4 is required

set -l rsync_ver (rsync --version |
                  string replace -rf '^rsync +version\D+([\d.]+) .*' '$1' |
                  string split .)

if test "$rsync_ver[1]" -gt 3 2>/dev/null
    or test "$rsync_ver[1]" -eq 3 -a "$rsync_ver[2]" -gt 2 2>/dev/null
    or test "$rsync_ver[1]" -eq 3 -a "$rsync_ver[2]" -eq 2 -a "$rsync_ver[3]" -gt 3 2>/dev/null
    set new_escaping 1
end

complete -c rsync -d "Remote path" -n "commandline -ct | string match -q '*:*'" -xa "
(
	# Prepend any user@host:/path information supplied before the remote completion.
        __rsync_remote_target
)(
	# Get the list of remote files from the specified rsync server.
        rsync --list-only (__rsync_remote_target) 2>/dev/null | string replace -r '^d.*' '\$0/' |
        string replace -r '(\S+\s+){4}' '' |
        string match --invert './' $(set -q new_escaping[1]; or echo ' | string escape -n'; echo)
)
"
