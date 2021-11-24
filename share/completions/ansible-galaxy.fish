# global options
complete -c ansible-galaxy -n __fish_use_subcommand -l version -d "Show version and config"
complete -c ansible-galaxy -s h -l help -d "Show help message"
complete -c ansible-galaxy -n __fish_use_subcommand -s v -l verbose -d "Verbose mode (-vvv for more, -vvvv for connection debugging)"

# first subcommand
complete -c ansible-galaxy -n __fish_use_subcommand -xa "collection\t'Manage a collection'
                                                         role\t'Manage a role'"

# second subcommand (for collection)
complete -c ansible-galaxy -n '__fish_seen_subcommand_from collection' -a "download\t'Download collections as tarball'
                                                                           init\t'Initialize new collection with the base structure'
                                                                           build\t'Build collection artifact that can be published'
                                                                           publish\t'Publish collection artifact to Ansible Galaxy'
                                                                           install\t'Install collections'
                                                                           list\t'Show collections installed'
                                                                           verify\t'Compare checksums of local and remote collections'"

# second subcommand (for role)
complete -c ansible-galaxy -n '__fish_seen_subcommand_from role' -a "init\t'Initialize new role with the base structure'
                                                                     remove\t'Delete roles from roles_path'
                                                                     delete\t'Removes the role from Galaxy'
                                                                     list\t'Show roles installed'
                                                                     search\t'Search the Galaxy database by keywords'
                                                                     import\t'Import role into a galaxy server'
                                                                     setup\t'Manage integration between Galaxy and the given source'
                                                                     info\t'View details about a role'
                                                                     install\t'Install roles'"
