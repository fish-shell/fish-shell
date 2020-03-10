function __fish_ipset_nosubcommand
    if __fish_seen_subcommand_from create add del test destroy list save restore flush rename swap help version
        return 1
    end
    return 0
end

function __fish_ipset_needs_setname
    if __fish_seen_subcommand_from add del test destroy list save restore flush rename swap
        return 0
    end
    return 1
end

function __fish_ipset_list_sets
    set -l ipset_list (ipset list --name 2>/dev/null)
    if not __fish_seen_subcommand_from $ipset_list
        echo $ipset_list
    end
end

complete -c ipset --no-files --condition __fish_ipset_nosubcommand -a create -d 'Create a set identified with SETNAME'
complete -c ipset --no-files --condition __fish_ipset_nosubcommand -a add -d 'Add a given entry to a SET'
complete -c ipset --no-files --condition __fish_ipset_nosubcommand -a del -d 'Delete an entry from a SET'
complete -c ipset --no-files --condition __fish_ipset_nosubcommand -a test -d 'Test whether an entry is in a set'
complete -c ipset --no-files --condition __fish_ipset_nosubcommand -a x -d 'Destroy the specified set or all sets'
complete -c ipset --no-files --condition __fish_ipset_nosubcommand -a destroy -d 'Destroy the specified set or all sets'
complete -c ipset --no-files --condition __fish_ipset_nosubcommand -a list -d a

complete -c ipset --no-files --condition __fish_ipset_needs_setname -a '(__fish_ipset_list_sets)'

complete -c ipset --no-files -s ! -o exist -d 'Ignore errors'
complete -c ipset --no-files -s o -o output -a 'plain save xml' -d 'Output format to the list command'
complete -c ipset --no-files -s q -o quiet -d 'Suppress any output'
complete -c ipset --no-files -s r -o resolve -d 'Enforce name lookup'
# complete -c ipset --no-files -s 's' -o 'sorted' -d 'Sorted output (not supported yet)'
complete -c ipset --no-files -s n -o name -d 'List just the names of the sets'
complete -c ipset --no-files -s t -o terse -d 'List the set names and headers'
complete -c ipset --no-files -s f -o file -d 'Filename instead of stdout'
