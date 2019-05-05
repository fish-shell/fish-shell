
# Options
complete -c cygport -s 4 -l 32 -d "Build for 32-bit Cygwin"
complete -c cygport -s 8 -l 64 -d "Build for 64-bit Cygwin"
complete -c cygport -l debug -d "Enable debugging messages"

# Cygport file
complete -c cygport -n '__fish_is_first_token' -f -a '*.cygport' -d "Cygport file"

# Commands
complete -c cygport -n 'not __fish_is_first_token' -f -a 'downloadall fetchall wgetall getall' -d "Download all sources"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'download fetch wget get' -d "Download missing sources"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'prep unpack' -d "Prepare source directory"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'compile build make' -d "Build software"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'check test' -d "Run test suite"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'inst install' -d "Install into DESTDIR and run post-installation steps"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'postinst' -d "Run post-installation steps"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'list' -d "List package files"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'listdebug listdbg' -d "List debug package files"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'dep' -d "Show dependencies"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'info' -d "Show packaging info"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'homepage web www' -d "Show project homepage URL"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'package-test pkg-test' -d "Create packages, marked as test"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'package pkg' -d "Create packages"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'diff mkdiff mkpatch' -d "Create source patches"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'upload up' -d "Upload finished packages"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'stage' -d "Upload packages without marking !ready"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'announce' -d "Send announcement email"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'clean finish' -d "Delete working directory"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'almostall all' -d "Same as prep build inst pkg"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'help' -d "Show help"
complete -c cygport -n 'not __fish_is_first_token' -f -a 'version' -d "Show version"
