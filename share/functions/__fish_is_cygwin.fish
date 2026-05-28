# localization: skip(private)
function __fish_is_cygwin
    # Based on https://github.com/msys2/msys2-runtime/blob/01d6c708f9221334d18ab332621b6d87eb12d37e/winsup/cygwin/uname.cc#L28
    # and https://cygwin.com/git/?p=newlib-cygwin.git;a=blob;f=winsup/cygwin/uname.cc;h=c08e30f97da9e6e30ec011dfdc40f37d14fb6465;hb=daabea98682f3f4bef0044829a8d24226135bb71#l40
    __fish_uname | string match -qr "^(MSYS|MINGW64|MINGW32|CYGWIN)"
end
