complete -c groupdel -xa '(__fish_complete_groups)' -d 'Delete a group'

complete -c groupdel -s f -l force -d 'force the removal'
complete -c groupdel -s h -l help -d 'display help message'
complete -c groupdel -s R -l root -xa '(__fish_complete_directories)' -d 'apply changes in a chroot directory'
complete -c groupdel -s P -l prefix -xa '(__fish_complete_directories)' -d 'apply changes in a prefix directory'
