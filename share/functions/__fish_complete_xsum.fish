function __fish_complete_xsum --description 'Complete md5sum sha1 etc' --argument-names cmd
    complete -c $cmd -d "Compute and check $cmd message digest" -r
    complete -c $cmd -s b -l binary -d 'Read in binary mode'
    complete -c $cmd -s c -l check  -d "Read $cmd sums from files and check them"
    complete -c $cmd -s t -l text   -d 'Read in text mode'
    complete -c $cmd -l quiet       -d 'Don\'t print OK for each successfully verified file'
    complete -c $cmd -l status      -d 'Don\'t output anything, status code shows success'
    complete -c $cmd -s w -l warn   -d 'Warn about improperly formatted checksum lines'
    complete -c $cmd -l strict      -d 'With --check, exit non-zero for any invalid input'
    complete -c $cmd -l help        -d 'Display help text'
    complete -c $cmd -l version     -d 'Output version information and exit'
end

