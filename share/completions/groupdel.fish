complete -c groupdel -xa '(__fish_complete_groups)' -d 'Delete a group'

complete -c groupdel -s f -l force -d 'Force the removal'
complete -c groupdel -s h -l help -d 'Display help message'
complete -c groupdel -s R -l root -xa '(__fish_complete_directories)' -d 'Apply changes in a chroot directory'
complete -c groupdel -s P -l prefix -xa '(__fish_complete_directories)' -d 'Apply changes in a prefix directory'
