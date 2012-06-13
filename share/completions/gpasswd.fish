
complete -c gpasswd -xa '(__fish_complete_groups)'
complete -c gpasswd -s a -l add -d 'Add user to group' -xa '(__fish_complete_users)'
complete -c gpasswd -s d -l delete -d 'Remove user from group' -xa '(__fish_complete_users)'
complete -c gpasswd -s h -l help -d 'Print help'
complete -c gpasswd -s r -l remove-password -d 'Remove the GROUP\'s password'
complete -c gpasswd -s R -l restrict -d 'Restrict access to GROUP to its members'
complete -c gpasswd -s M -l members  -d 'Set the list of members of GROUP' -xa '(__fish_complete_list , __fish_complete_users)'
complete -c gpasswd -s A -l administrators -d 'set the list of administrators for GROUP' -xa '(__fish_complete_list , __fish_complete_users)'

