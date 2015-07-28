set __fish_facl_spec_keywords default user group mask other

function __fish_facl_list_spec_keyword
  for keyword in $__fish_facl_spec_keywords
    echo $keyword:
  end
end

function __fish_facl_starts_with_spec_user
  echo (commandline -ct) | __fish_sgrep -q -E 'u(ser)?:'
end

function __fish_facl_starts_with_spec_group
  echo (commandline -ct) | __fish_sgrep -q -E 'g(roup)?:'
end

function __fish_facl_extract_acl
  echo (commandline -ct) | __fish_sgrep -o -E '\w*:'
end

complete -c setfacl    -s m -s x -l modify -l remove -l set -n '__fish_facl_starts_with_spec_user'  -a '(__fish_facl_extract_acl)(__fish_complete_users  | sed "s/\t/:\t/g")'
complete -c setfacl    -s m -s x -l modify -l remove -l set -n '__fish_facl_starts_with_spec_group' -a '(__fish_facl_extract_acl)(__fish_complete_groups | sed "s/\t/:\t/g")'
complete -c setfacl -f -s m -s x -l modify -l remove -l set -a '(__fish_facl_list_spec_keyword)'

complete -c setfacl    -s b -l remove-all     --description 'Remove all extended ACL entries'
complete -c setfacl    -s k -l remove-default --description 'Remove the Default ACL'
complete -c setfacl    -s n -l no-mask        --description 'Do not recalculate the effective rights mask'
complete -c setfacl         -l mask           --description 'Do recalculate the effective rights mask'
complete -c setfacl    -s d -l default        --description 'All operations apply to the Default ACL'
complete -c setfacl         -l restore        --description 'Restore a permission backup created by `getfacl -R\' or similar'
complete -c setfacl         -l test           --description 'Test mode'
complete -c setfacl    -s R -l recursive      --description 'Apply operations to all files and directories recursively'
complete -c setfacl    -s L -l logical        --description 'Logical walk, follow symbolic links to directories'
complete -c setfacl    -s P -l physical       --description 'Physical walk, do not follow symbolic links to directories'
complete -c setfacl -f -s v -l version        --description 'Print the version of setfacl and exit'
complete -c setfacl -f -s h -l help           --description 'Print help explaining the command line options'
complete -c setfacl    -s m -l modify         --description 'Modify the current ACL(s) of file(s)'
complete -c setfacl    -s x -l remove         --description 'Remove entries from the ACL(s) of file(s)'
complete -c setfacl    -s M -l modify-file    --description 'Read ACL entries to modify from file'
complete -c setfacl    -s X -l remove-file    --description 'Read ACL entries to remove from file'
complete -c setfacl         -l set-file       --description 'Read ACL entries to set from file'
complete -c setfacl         -l set            --description 'Set the ACL of file(s), replacing the current ACL'
