complete -c usermod -xa "(__fish_complete_users)"
complete -c usermod -s a -l append -d "Append groups (use with -G)"
complete -c usermod -s c -l comment -d "Change user's password file comment" -x
complete -c usermod -s d -l home -d "Change user's login directory" -r
complete -c usermod -s e -l expiredate -d "Date (YYYY-MM-DD) on which the user account will be disabled" -x
complete -c usermod -s f -l inactive -d "Number of days after a password expires until the account is locked" -xa "(seq 0 365)"
complete -c usermod -s g -l gid -d "Group name or number of the user's new initial login group" -xa "(__fish_complete_groups)"
complete -c usermod -s G -l groups -d "List of groups which the user is also a member of" -xa "(__fish_complete_list , __fish_complete_groups)"
complete -c usermod -s l -l login -d "Change user's name" -x
complete -c usermod -s L -l lock -d "Lock user's password" -f
complete -c usermod -s m -l move-home -d "Move the content of the user's home directory to the new location" -f
complete -c usermod -s o -l non-unique -d "Allow non-unique UID" -f
complete -c usermod -s p -l password -d "The encrypted password, as returned by crypt(3)" -x
complete -c usermod -s R -l root -d "Apply changes in this directory" -x
complete -c usermod -s P -l prefix -d "Apply changes in the PREFIX_DIR" -x
complete -c usermod -s s -l shell -d "The name of the user's new login shell" -x
complete -c usermod -s u -l uid -d "The new numerical value of the user's ID" -x
complete -c usermod -s U -l unlock -d "Unlock a users password" -f
complete -c usermod -s v -l add-subuids -d "Add a range of subordinate uids to the user's account" -x
complete -c usermod -s V -l del-subuids -d "Remove a range of subordinate uids from the user's account" -x
complete -c usermod -s w -l add-subgids -d "Add a range of subordinate gids to the user's account" -x
complete -c usermod -s W -l del-subgids -d "Remove a range of subordinate gids from the user's account" -x
complete -c usermod -s Z -l selinux-user -d "The new SELinux user for the user's login" -x
