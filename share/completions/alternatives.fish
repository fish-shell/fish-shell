function __fish_print_alternatives_names -d "Get the names of link groups in the alternatives system"
    alternatives --list | cut -f 1 | string trim
end

# common options

complete -c alternatives -l verbose -d "Generate more comments about what alternatives is doing"
complete -c alternatives -l help -d "Give some usage information"
complete -c alternatives -l version -d "Tell which version of alternatives this is"
complete -c alternatives -l keep-missing -d "If new variant doesn't provide some files, keep previous links"
complete -c alternatives -l altdir -xa "(__fish_complete_directories)" -d "Specifies the alternatives directory"
complete -c alternatives -l admindir -xa "(__fish_complete_directories)" -d "Specifies the administrative directory"

# actions

complete -c alternatives -l install -r -d "Add a group of alternatives to the system"
complete -c alternatives -l slave -n "__fish_contains_opt install" -r -d "Add a slave link to the new group"
complete -c alternatives -l initscript -n "__fish_contains_opt install" -F -d "Add an initscript for the new group"
complete -c alternatives -l family -n "__fish_contains_opt install" -x -d "Set a family for the new group"

complete -c alternatives -l remove -ra "(__fish_print_alternatives_names)" -d "Remove an alternative and all of its associated slave links"
complete -c alternatives -l set -ra "(__fish_print_alternatives_names)" -d "Set link group to given path"
complete -c alternatives -l config -xa "(__fish_print_alternatives_names)" -d "Open menu to configure link group"
complete -c alternatives -l auto -xa "(__fish_print_alternatives_names)" -d "Switch the master symlink name to automatic mode"
complete -c alternatives -l display -xa "(__fish_print_alternatives_names)" -d "Display information about the link group"
complete -c alternatives -l list -f -d "Display information about all link groups"
complete -c alternatives -l remove-all -xa "(__fish_print_alternatives_names)" -d "Remove the whole link group name"
complete -c alternatives -l add-slave -ra "(__fish_print_alternatives_names)" -d "Add a slave link to an existing alternative"
complete -c alternatives -l remove-slave -ra "(__fish_print_alternatives_names)" -d "Remove slave from an existing alternative"
