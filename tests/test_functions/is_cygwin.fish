function is_cygwin
    __fish_uname | string match -qr "^(MSYS|CYGWIN)"
end
