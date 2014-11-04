function __fish_complete_xsum --description 'Complete md5sum sha1 etc' --argument-names cmd
    complete -c $cmd -d "Compute and check $cmd message digest" -r
    complete -c $cmd -s b -l binary --description 'Read in binary mode'
    complete -c $cmd -s c -l check  --description "Read $cmd sums from files and check them"
    complete -c $cmd -s t -l text   --description 'Read in text mode'
    complete -c $cmd -l quiet       --description 'Don\'t print OK for each successfully verified file'
    complete -c $cmd -l status      --description 'Don\'t output anything, status code shows success'
    complete -c $cmd -s w -l warn   --description 'Warn about improperly formatted checksum lines'
    complete -c $cmd -l strict      --description 'With --check, exit non-zero for any invalid input'
    complete -c $cmd -l help        --description 'Display help text'
    complete -c $cmd -l version     --description 'Output version information and exit'
end

