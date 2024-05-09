complete -c duf -l all -o all -d "include pseudo, duplicate, inaccessible file systems"
complete -c duf -l avail-threshold -o avail-threshold -d "specifies the coloring threshold (yellow, red) of the avail column, must be integer with optional SI prefixes (default '10G,1G')" -x
complete -c duf -l hide -o hide -d "hide specific devices, separated with commas" -x
complete -c duf -l hide-fs -o hide-fs -d "hide specific filesystems, separated with commas" -x
complete -c duf -l hide-mp -o hide-mp -d "hide specific mount points, separated with commas (supports wildcards)" -x
complete -c duf -l inodes -o inodes -d "list inode information instead of block usage"
complete -c duf -l json -o json -d "output all devices in JSON format"
complete -c duf -l only -o only -d "show only specific devices, separated with commas" -xa "local network fuse special loops binds"
complete -c duf -l only-fs -o only-fs -d "only specific filesystems, separated with commas" -x
complete -c duf -l only-mp -o only-mp -d "only specific mount points, separated with commas (supports wildcards)" -x
complete -c duf -l output -o output -d "output fields, separated with commas" -xa "mountpoint size used avail usage inodes inodes_used inodes_avail inodes_usage type filesystem"
complete -c duf -l sort -o sort -d "sort output (default 'mountpoint')" -xa "mountpoint size used avail usage inodes inodes_used inodes_avail inodes_usage type filesystem"
complete -c duf -l style -o style -d "style (default 'unicode')" -xa "unicode ascii"
complete -c duf -l theme -o theme -d "color themes (default 'light')" -xa "dark light ansi"
complete -c duf -l usage-threshold -o usage-threshold -d "specifies the coloring threshold (yellow, red) of the usage bars as a floating point number from 0 to 1 (default '0.5,0.9')" -x
complete -c duf -l version -o version -d "display version"
complete -c duf -l warnings -o warnings -d "output all warnings to STDERR"
complete -c duf -l width -o width -d "max output width" -x
