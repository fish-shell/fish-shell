# mkdosfs is a tool to create MS-DOS filesystems.
# See: https://github.com/dosfstools/dosfstools

complete -c mkdosfs -s a -d 'Disable alignment'
complete -c mkdosfs -s A -d 'Use Atari variation'
complete -c mkdosfs -s b -x -d 'Select location of backup sector'
complete -c mkdosfs -s c -d 'Check for bad blocks'
complete -c mkdosfs -s C -d 'Create file given as device'
complete -c mkdosfs -s D -x -d 'Specify drive number' -a '0x80\tHard\ disk 0x00\tFloppy\ disk'
complete -c mkdosfs -s f -x -d 'Specify number of FATs' -a '1\t 2\tDefault 3\t 4\t'
complete -c mkdosfs -s F -x -d 'Specify FAT size' -a '12 16 32'
complete -c mkdosfs -s h -x -d 'Specify number of hidden sectors'
complete -c mkdosfs -s i -x -d 'Specify volume ID'
complete -c mkdosfs -s I -d 'Force to use entire disk'
complete -c mkdosfs -s l -r -d 'Read bad blocks list from given file'
complete -c mkdosfs -s m -r -d 'Specify file containing non bootable message' -a '-\tStdin'
complete -c mkdosfs -s M -x -d 'Specify media type' -a '
    0xf0\t2.88MB3.5-inch,\ 2-sided,\ 36-sector
    0xf0\t1.44MB\ 3.5-inch,\ 2-sided,\ 18-sector
    0xf9\t720KB\ 3.5-inch,\ 2-sided,\ 9-sector
    0xf9\t1.2MB\ 5.25-inch,\ 2-sided,\ 15-sector
    0xfd\t360KB\ 5.25-inch,\ 2-sided,\ 9-sector
    0xff\t320KB\ 5.25-inch,\ 2-sided,\ 8-sector
    0xfc\t180KB\ 5.25-inch,\ 1-sided,\ 9-sector
    0xfe\t160KB\ 5.25-inch,\ 1-sided,\ 8-sector
    0xf8\tFixed\ disk
'
complete -c mkdosfs -s n -x -d 'Specify volume name'
complete -c mkdosfs -s r -x -d 'Specify number of entries available in root directory' -a '112\tFloppy\ default 224\tFloppy\ default 512\tHard\ disks\ default'
complete -c mkdosfs -s R -x -d 'Specify number of reserved sectors'
complete -c mkdosfs -s s -x -d 'Specify number of sector per cluster' -a '1 2 4 8 16 32 64 128'
complete -c mkdosfs -s S -x -d 'Specify number of bytes per logical sector' -a '512 1024 2048 4096 8192 16384 32768'
complete -c mkdosfs -s v -d 'Verbose mode'
complete -c mkdosfs -l invariant -d 'Use constants for randomly generated data'
complete -c mkdosfs -l help -d 'Show help and exit'
