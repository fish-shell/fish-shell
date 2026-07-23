# dseditgroup - group record manipulation tool (man 8 dseditgroup)
#
# dseditgroup uses flag-style options; the -o flag selects the operation
# (read, create, delete, edit, checkmember).  Most parameters are independent
# of which operation is selected.

# ── live enumerators ──────────────────────────────────────────────────────────

# Local group names from dscl (fast, unprivileged)
function __fish_dseditgroup_groups
    dscl . -list /Groups 2>/dev/null
end

# Local user names from dscl (fast, unprivileged)
function __fish_dseditgroup_users
    dscl . -list /Users 2>/dev/null
end

# ── options ───────────────────────────────────────────────────────────────────

# -o operation (exclusive values)
complete -c dseditgroup -s o -x -d 'Operation to perform' \
    -a 'read\t"Display parameters of the group record"
create\t"Create a new group record"
delete\t"Delete a group record"
edit\t"Edit a group record"
checkmember\t"Check if a user is a member of the group"'

# -n nodename
complete -c dseditgroup -s n -x -d 'Directory Service node (e.g. /LDAPv3/ldap.example.com or . for local)'

# Authentication
complete -c dseditgroup -s u -x -d 'Admin username to authenticate as'
complete -c dseditgroup -s P -x -d 'Authentication password (on command line)'
complete -c dseditgroup -s p -f -d 'Prompt interactively for authentication password'

# Behaviour flags
complete -c dseditgroup -s q -f -d 'Disable interactive verification of replace/delete operations'
complete -c dseditgroup -s v -f -d 'Enable verbose logging of DirectoryService API calls'
complete -c dseditgroup -s L -f -d 'Maintain ComputerLists in parallel with ComputerGroups'

# -m member (for checkmember; complete from local users)
complete -c dseditgroup -s m -x -d 'Username to verify group membership (used with -o checkmember)' \
    -a '(__fish_dseditgroup_users)'

# -a / -d record name (add / delete record from group)
complete -c dseditgroup -s a -x -d 'Name of record to add to the group'
complete -c dseditgroup -s d -x -d 'Name of record to delete from the group'

# -t recordtype (type of record for -a / -d)
complete -c dseditgroup -s t -x -d 'Type of record to add or delete' \
    -a 'user\t"User account record"
computer\t"Computer record"
group\t"Group record"
computergroup\t"Computer group record"'

# -T grouptype (type of group to create or modify)
complete -c dseditgroup -s T -x -d 'Type of group record to create or modify' \
    -a 'group\t"Standard group"
computergroup\t"Computer group"'

# Numeric / GUID / SID attributes
complete -c dseditgroup -s i -x -d 'Group ID (gid) to add or replace'
complete -c dseditgroup -s g -x -d 'GUID (128-bit text representation) to add or replace'
complete -c dseditgroup -s S -x -d 'SID to add or replace'

# Text attributes
complete -c dseditgroup -s r -x -d 'Real name (display name) to add or replace'
complete -c dseditgroup -s k -x -d 'Keyword to add'
complete -c dseditgroup -s c -x -d 'Comment to add or replace'
complete -c dseditgroup -s s -x -d 'Time-to-live in seconds to add or replace'

# -f format
complete -c dseditgroup -s f -x -d 'Change group format' \
    -a 'n\t"New group format"
l\t"Legacy group format"'

# Trailing groupname argument — complete from known local groups
complete -c dseditgroup -f -a '(__fish_dseditgroup_groups)' -d 'Group name'
