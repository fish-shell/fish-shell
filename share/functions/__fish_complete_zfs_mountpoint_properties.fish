function __fish_complete_zfs_mountpoint_properties -d "Completes with ZFS mountpoint properties"
    set -l OS ""
    switch (uname)
        case Linux
            set OS Linux
        case Darwin
            set OS macOS
        case FreeBSD
            set OS FreeBSD
        case SunOS
            set OS SunOS
            # Others?
        case "*"
            set OS unknown
    end
    echo -e "atime\tUpdate access time on read (on, off)"
    echo -e "devices\tAre contained device nodes openable (on, off)"
    echo -e "exec\tCan contained executables be executed (on, off)"
    echo -e "readonly\tRead-only (on, off)"
    echo -e "setuid\tRespect set-UID bit (on, off)"
    if test $OS = SunOS
        echo -e "nbmand\tMount with Non Blocking mandatory locks (on, off)"
        echo -e "xattr\tExtended attributes (on, off, sa)"
    else if test $OS = Linux
        echo -e "nbmand\tMount with Non Blocking mandatory locks (on, off)"
        echo -e "relatime\tSometimes update access time on read (on, off)"
        echo -e "xattr\tExtended attributes (on, off, sa)"
    end
end
