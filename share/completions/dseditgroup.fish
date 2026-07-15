function __fish_dseditgroup_groups
    dscl . -list /Groups
end

function __fish_dseditgroup_users
    dscl . -list /Users
end

complete -c dseditgroup -s o -x -d 'Operation to perform' -a '
read\t"Display parameters of the group record"
create\t"Create a new group record"
delete\t"Delete a group record"
edit\t"Edit a group record"
checkmember\t"Check if a user is a member of the group"'
complete -c dseditgroup -s n -x -d 'Directory Service node (e.g. /LDAPv3/ldap.example.com or . for local)'
complete -c dseditgroup -s u -x -d 'Admin username to authenticate as'
complete -c dseditgroup -s P -x -d 'Authentication password (on command line)'
complete -c dseditgroup -s p -d 'Prompt interactively for authentication password'
complete -c dseditgroup -s q -d 'Disable interactive verification of replace/delete operations'
complete -c dseditgroup -s v -d 'Enable verbose logging of DirectoryService API calls'
complete -c dseditgroup -s L -d 'Maintain ComputerLists in parallel with ComputerGroups'
complete -c dseditgroup -d 'Username to verify group membership (used with -o checkmember)' -s m -x -a '(__fish_dseditgroup_users)'
complete -c dseditgroup -s a -x -d 'Name of record to add to the group'
complete -c dseditgroup -s d -x -d 'Name of record to delete from the group'
complete -c dseditgroup -s t -x -d 'Type of record to add or delete' -a 'user\t"User account record"
computer\t"Computer record"
group\t"Group record"
computergroup\t"Computer group record"'
complete -c dseditgroup -s T -x -d 'Type of group record to create or modify' -a '
group\t"Standard group"
computergroup\t"Computer group"'
complete -c dseditgroup -s i -x -d 'Group ID (gid) to add or replace'
complete -c dseditgroup -s g -x -d 'GUID (128-bit text representation) to add or replace'
complete -c dseditgroup -s S -x -d 'SID to add or replace'
complete -c dseditgroup -s r -x -d 'Real name (display name) to add or replace'
complete -c dseditgroup -s k -x -d 'Keyword to add'
complete -c dseditgroup -s c -x -d 'Comment to add or replace'
complete -c dseditgroup -s s -x -d 'Time-to-live in seconds to add or replace'
complete -c dseditgroup -s f -x -d 'Change group format' -a '
n\t"New group format"
l\t"Legacy group format"'
complete -c dseditgroup -f -a '(__fish_dseditgroup_groups)' -d 'Group name'
