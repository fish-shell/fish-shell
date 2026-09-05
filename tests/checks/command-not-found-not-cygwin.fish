#RUN: fish=%fish %fish %s
#REQUIRES: %fish -c 'not cygwin_noacl .'
#REQUIRES: %fish -c 'not cygwin_nosymlinks'

# EACCES -> status 126
$fish -c 'touch file_8de9325e && chmod 000 file_8de9325e && ./file_8de9325e; echo $status && rm -f file_8de9325e'
#CHECKERR: fish: Unknown command './file_8de9325e': Missing execute permissions for the file or one of its parent directories.
#CHECKERR: touch file_8de9325e && chmod 000 file_8de9325e && ./file_8de9325e; echo $status && rm -f file_8de9325e
#CHECKERR:                                                   ^~~~~~~~~~~~~~^
#CHECK: 126

# ELOOP -> status 126
$fish -c 'ln -s symlink_8de9325e symlink_8de9325e && ./symlink_8de9325e; echo $status && rm symlink_8de9325e'
#CHECKERR: fish: Unknown command './symlink_8de9325e': Too many levels of symbolic links. Maybe a loop?
#CHECKERR: ln -s symlink_8de9325e symlink_8de9325e && ./symlink_8de9325e; echo $status && rm symlink_8de9325e
#CHECKERR:                                            ^~~~~~~~~~~~~~~~~^
#CHECK: 126
