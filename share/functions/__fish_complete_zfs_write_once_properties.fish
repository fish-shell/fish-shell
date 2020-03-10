function __fish_complete_zfs_write_once_properties -d "Completes with ZFS properties which can only be written at filesystem creation, and only be read thereafter"
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
    echo -e "normalization\tUnicode normalization (none, formC, formD, formKC, formKD)"
    echo -e "utf8only\tReject non-UTF-8-compliant filenames (on, off)"
    if test $OS = Linux
        echo -e "overlay\tAllow overlay mount (on, off)"
        if command -sq sestatus # SELinux is enabled
            echo -e "context\tSELinux context for the child filesystem"
            echo -e "fscontext\tSELinux context for the filesystem being mounted"
            echo -e "defcontext\tSELinux context for unlabeled files"
            echo -e "rootcontext\tSELinux context for the root inode of the filesystem"
        end
        echo -e "casesensitivity\tCase sensitivity (sensitive, insensitive)"
    else
        echo -e "casesensitivity\tCase sensitivity (sensitive, insensitive, mixed)"
    end
end
