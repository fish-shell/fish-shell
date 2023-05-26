complete -c age-keygen -s o -l output -n "not __fish_contains_opt -s o output" -d "output file for secret key"
complete -c age-keygen -s y -n "not __fish_contains_opt -s y" -d "read identity file, print recipient(s)"
complete -c age -l version -d "print version number"
