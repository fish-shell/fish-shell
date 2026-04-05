# localization: skip(private)
function __fish_cygwin_noacl
    # MSYS (default) and Cygwin (non-default) mounts may not support POSIX permissions.
    __fish_is_cygwin
    and { mount | string match "*on $(stat -c %m -- $argv[1]) *" | string match -qr "[(,]noacl[),]" }
end
