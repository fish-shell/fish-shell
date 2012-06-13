complete -c lppasswd -d 'Change CUPS digest password' -xa '(__fish_complete_users)'
complete -c lppasswd -s a -d 'Add password'
complete -c lppasswd -s x -d 'Remove password'
complete -c lppasswd -s g -d 'Specify the group other, than default system group' -xa '(__fish_complete_groups)'
