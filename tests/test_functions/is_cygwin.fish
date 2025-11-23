function is_cygwin
    string match -qr "^(MSYS|CYGWIN)" -- (uname)
end
