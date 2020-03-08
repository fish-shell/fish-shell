function __fish_facl_list_spec_keyword
    for keyword in default user group mask other
        echo $keyword:
    end
end

function __fish_facl_starts_with_spec_user
    commandline -ct | string match -r "u(ser)?:"
end

function __fish_facl_starts_with_spec_group
    commandline -ct | string match -r "g(roup)?:"
end

function __fish_facl_extract_acl
    commandline -ct | string replace -ar '.*(\w*:).*' '$1'
end

complete -c setfacl -s m -s x -l modify -l remove -l set -n __fish_facl_starts_with_spec_user -a '(__fish_facl_extract_acl)(__fish_complete_users  | string replace -a "\t" ":\t")'
complete -c setfacl -s m -s x -l modify -l remove -l set -n __fish_facl_starts_with_spec_group -a '(__fish_facl_extract_acl)(__fish_complete_groups | string replace -a "\t" ":\t")'
complete -c setfacl -f -s m -s x -l modify -l remove -l set -a '(__fish_facl_list_spec_keyword)'

complete -c setfacl -s b -l remove-all -d 'Remove all extended ACL entries'
complete -c setfacl -s k -l remove-default -d 'Remove the Default ACL'
complete -c setfacl -s n -l no-mask -d 'Do not recalculate the effective rights mask'
complete -c setfacl -l mask -d 'Do recalculate the effective rights mask'
complete -c setfacl -s d -l default -d 'All operations apply to the Default ACL'
complete -c setfacl -l restore -d 'Restore a permission backup created by `getfacl -R\' or similar'
complete -c setfacl -l test -d 'Test mode'
complete -c setfacl -s R -l recursive -d 'Apply operations to all files and directories recursively'
complete -c setfacl -s L -l logical -d 'Logical walk, follow symbolic links to directories'
complete -c setfacl -s P -l physical -d 'Physical walk, do not follow symbolic links to directories'
complete -c setfacl -f -s v -l version -d 'Print the version of setfacl and exit'
complete -c setfacl -f -s h -l help -d 'Print help explaining the command line options'
complete -c setfacl -s m -l modify -d 'Modify the current ACL(s) of file(s)'
complete -c setfacl -s x -l remove -d 'Remove entries from the ACL(s) of file(s)'
complete -c setfacl -s M -l modify-file -d 'Read ACL entries to modify from file'
complete -c setfacl -s X -l remove-file -d 'Read ACL entries to remove from file'
complete -c setfacl -l set-file -d 'Read ACL entries to set from file'
complete -c setfacl -l set -d 'Set the ACL of file(s), replacing the current ACL'
