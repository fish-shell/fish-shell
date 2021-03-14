function __fish_mkpasswd_methods --description "Complete hashing methods for mkpasswd"
    mkpasswd -m help | tail -n +2 | string replace -r '^(\S+)\s+(\S.*)' '$1\t$2'
    echo -e "help\tList available methods"
end

complete -c mkpasswd -f
complete -c mkpasswd -s S -l salt -x -d 'Use given string as salt'
complete -c mkpasswd -s R -l rounds -x -d 'Use given number of rounds'
complete -c mkpasswd -s m -l method -xa "(__fish_mkpasswd_methods)" -d 'Compute the password using the given method'
complete -c mkpasswd -s 5 -d 'Like --method=md5crypt'
complete -c mkpasswd -s P -l password-fd -x -d 'Read the password from the given file descriptor'
complete -c mkpasswd -s s -l stdin -d 'Read the password from stdin'
