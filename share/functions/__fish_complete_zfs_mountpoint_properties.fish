function __fish_complete_zfs_mountpoint_properties -d "Completes with ZFS mountpoint properties"
    set -l OS ""
    switch (uname)
        case Linux
            set OS "Linux"
        case Darwin
            set OS "macOS"
        case FreeBSD
            set OS "FreeBSD"
        case SunOS
            set OS "SunOS"
            # Others?
        case "*"
            set OS "unknown"
    end
    echo -e "atime\t"(_ "Update access time on read")" (on, off)"
    echo -e "devices\t"(_ "Are contained device nodes openable")" (on, off)"
    echo -e "exec\t"(_ "Can contained executables be executed")" (on, off)"
    echo -e "readonly\t"(_ "Read-only")" (on, off)"
    echo -e "setuid\t"(_ "Respect set-UID bit")" (on, off)"
    if test $OS = "SunOS"
        echo -e "nbmand\t"(_ "Mount with Non Blocking mandatory locks")" (on, off)"
        echo -e "xattr\t"(_ "Extended attributes")" (on, off, sa)"
    else if test $OS = "Linux"
        echo -e "nbmand\t"(_ "Mount with Non Blocking mandatory locks")" (on, off)"
        echo -e "relatime\t"(_ "Sometimes update access time on read")" (on, off)"
        echo -e "xattr\t"(_ "Extended attributes")" (on, off, sa)"
    end
end
