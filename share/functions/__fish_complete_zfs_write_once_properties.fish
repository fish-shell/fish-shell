function __fish_complete_zfs_write_once_properties -d "Completes with ZFS properties which can only be written at filesystem creation, and only be read thereafter"
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
    echo -e "normalization\t"(_ "Unicode normalization")" (none, formC, formD, formKC, formKD)"
    echo -e "utf8only\t"(_ "Reject non-UTF-8-compliant filenames")" (on, off)"
    if test $OS = "Linux"
        echo -e "overlay\t"(_ "Allow overlay mount")" (on, off)"
        if command -sq sestatus # SELinux is enabled
            echo -e "context\t"(_ "SELinux context for the child filesystem")
            echo -e "fscontext\t"(_ "SELinux context for the filesystem being mounted")
            echo -e "defcontext\t"(_ "SELinux context for unlabeled files")
            echo -e "rootcontext\t"(_ "SELinux context for the root inode of the filesystem")
        end
        echo -e "casesensitivity\t"(_ "Case sensitivity")" (sensitive, insensitive)"
    else
        echo -e "casesensitivity\t"(_ "Case sensitivity")" (sensitive, insensitive, mixed)"
    end
end
