
complete -c cygport -s 4 -l 32
complete -c cygport -s 8 -l 64
complete -c cygport -l debug
complete -c cygport -n '__fish_is_first_token' -A -f -a '*.cygport'
complete -c cygport -n 'not __fish_is_first_token' -A -f -a 'downloadall fetchall wgetall getall download fetch wget get prep unpack compile build make check test inst postint list listdebug listdbg dep info homepage web www package pkg diff mkdiff mkpatch upload up ci clean finish almostall all help version'
