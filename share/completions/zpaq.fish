#zpaq 7.15

#variables
set -l seen __fish_seen_subcommand_from
set -l commands a add l list x extract
set -l command1 a add
set -l command2 l list
set -l command3 x extract

#commands
complete -c zpaq -f -n "not $seen $commands" -a "$commands"
complete -c zpaq -f -n "not $seen $commands" -a a -d add
complete -c zpaq -f -n "not $seen $commands" -a add -d "Append files to archive if dates have changed"
complete -c zpaq -f -n "not $seen $commands" -a x -d extract
complete -c zpaq -f -n "not $seen $commands" -a extract -d "Extract most recent versions of files"
complete -c zpaq -f -n "not $seen $commands" -a l -d list
complete -c zpaq -f -n "not $seen $commands" -a list -d "List or compare external files to archive"

#options
complete -c zpaq -x -n "$seen $commands" -o all -a N -d "Extract/list versions in N [4] digit directories"
complete -c zpaq -f -n "$seen $commands" -s f -d -force
complete -c zpaq -f -n "$seen $command1" -o force -d "Append files if contents have changed"
complete -c zpaq -f -n "$seen $command2" -o force -d "Compare file contents instead of dates"
complete -c zpaq -f -n "$seen $command3" -o force -d "Overwrite existing output files"
complete -c zpaq -x -n "$seen $commands" -o fragment -a N -d "Set the dedupe fragment size (64 2^N to 8128 2^N bytes)"
complete -c zpaq -x -n "$seen $command1" -o index -a F -d "Create suffix for archive indexed by F, update F"
complete -c zpaq -x -n "$seen $command3" -o index -a F -d "Create index F for archive"
complete -c zpaq -x -n "$seen $commands" -o key -a X -d "Create or access encrypted archive with password X"
complete -c zpaq -x -n "$seen $command1" -o mN -d "-method N"
complete -c zpaq -x -n "$seen $command1" -o method -a N -d "Compress level N (0..5 = faster..better, default 1)"
complete -c zpaq -f -n "$seen $commands" -o noattributes -d "Ignore/don't save file attributes or permissions"
complete -c zpaq -r -n "$seen $commands" -o not -d "Exclude. * and ? match any string or char"
complete -c zpaq -r -n "$seen $command2" -o not -d "Exclude. =[+-#^?] exclude by comparison result"
complete -c zpaq -r -n "$seen $commands" -o only -d "Include only matches (default: *)"
complete -c zpaq -x -n "$seen $command3" -o repack -a F -d "Extract to new archive F with key [X] (default: none)"
complete -c zpaq -x -n "$seen $commands" -o sN -d "-summary N"
complete -c zpaq -x -n "$seen $commands" -o summary -a N -d "If N > 0 show brief progress"
complete -c zpaq -x -n "$seen $command2" -o summary -a N -d "Show top N sorted by size. -1: show frag IDs"
complete -c zpaq -f -n "$seen $command3" -o test -d "Verify but do not write files"
complete -c zpaq -x -n "$seen $commands" -o tN -d "-threads N"
complete -c zpaq -x -n "$seen $commands" -o threads -a N -d "Use N threads (default: 0 = all cores)"
complete -c zpaq -r -n "$seen $commands" -o to -a P -d "Rename files... to P... or all to P/all"
complete -c zpaq -x -n "$seen $commands" -o until -a N -d "Roll back archive to N'th update or -N from end"
complete -c zpaq -x -n "$seen $commands" -o until -a D -d "Set date D, roll back (UT, default time: 235959)"
