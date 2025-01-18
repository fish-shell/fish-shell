complete -f -c git -a subtree -d 'Manage git subtrees'
# Git subtree common completions
complete -f -c git -n '__fish_git_using_command subtree' -s q -l quiet -d 'Suppress output'
complete -f -c git -n '__fish_git_using_command subtree' -s d -l debug -d 'Debug output'
complete -f -c git -n '__fish_git_using_command subtree' -s P -l path -d 'Path to the subtree'

# Git subtree subcommands
complete -f -c git -n '__fish_git_using_command subtree' -a add -d "Add a new subtree to the repository"
complete -f -c git -n '__fish_git_using_command subtree' -a merge -d "Merge changes from a subtree into the repository"
complete -f -c git -n '__fish_git_using_command subtree' -a split -d "Extract a subtree from the repository"
complete -f -c git -n '__fish_git_using_command subtree' -a pull -d "Fetch and integrate changes from a remote subtree"
complete -f -c git -n '__fish_git_using_command subtree' -a push -d "Push changes to a remote subtree"

# Completions for push and split subcommands
complete -f -c git -n '__fish_git_using_command subtree; and __fish_seen_subcommand_from push split' -l annotate -d 'Annotate the commit'
complete -f -c git -n '__fish_git_using_command subtree; and __fish_seen_subcommand_from push split' -s b -l branch -d 'Branch to split'
complete -f -c git -n '__fish_git_using_command subtree; and __fish_seen_subcommand_from push split' -l ignore-joins -d 'Ignore joins during history reconstruction'
complete -f -c git -n '__fish_git_using_command subtree; and __fish_seen_subcommand_from push split' -l onto -d 'Specify the commit ID to start history reconstruction'
complete -f -c git -n '__fish_git_using_command subtree; and __fish_seen_subcommand_from push split' -l rejoin -d 'Merge the synthetic history back into the main project'
complete -f -c git -n '__fish_git_using_command subtree; and __fish_seen_subcommand_from push split; and __fish_contains_opt rejoin' -l squash -d 'Merge subtree changes as a single commit'
complete -f -c git -n '__fish_git_using_command subtree; and __fish_seen_subcommand_from push split; and __fish_contains_opt rejoin' -l no-squash -d 'Do not merge subtree changes as a single commit'
complete -f -c git -n '__fish_git_using_command subtree; and __fish_seen_subcommand_from push split; and __fish_contains_opt rejoin' -s m -l message -d 'Use the given message as the commit message for the merge commit'

# Completion for add and merge subcommands
complete -f -c git -n '__fish_git_using_command subtree; and __fish_seen_subcommand_from add merge' -l squash -d 'Merge subtree changes as a single commit'
complete -f -c git -n '__fish_git_using_command subtree; and __fish_seen_subcommand_from add merge' -l no-squash -d 'Do not merge subtree changes as a single commit'
complete -f -c git -n '__fish_git_using_command subtree; and __fish_seen_subcommand_from add merge' -s m -l message -d 'Use the given message as the commit message for the merge commit'
