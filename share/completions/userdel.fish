complete -c userdel -xa "(__fish_complete_users)"

complete -c userdel -s f -l force -d "force the removal"
complete -c userdel -s h -l help -d "display help message"
complete -c userdel -s r -l remove -d "remove user's home and mail directories"
complete -c userdel -s R -l root -xa "(__fish_complete_directories)" -d "apply changes in a chroot directory"
complete -c userdel -s P -l prefix -xa "(__fish_complete_directories)" -d "apply changes in a prefix directory"
complete -c userdel -s Z -l selinux-user -d "remove SELinux user mappings"
