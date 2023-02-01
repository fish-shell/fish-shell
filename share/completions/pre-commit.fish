set -l commands autoupdate clean gc init-templatedir install install-hooks migrate-config run sample-config try-repo uninstall validate-config validate-manifest
set -l hook_stages commit merge-commit prepare-commit-msg commit-msg post-commit manual post-checkout push post-merge post-rewrite
set -l hook_types pre-commit pre-merge-commit pre-push prepare-commit-msg commit-msg post-commit post-checkout post-merge post-rewrite

functions -q __fish_git || source $__fish_data_dir/completions/git.fish

function __fish_pre_commit_config_print -a key
    set -l config (__fish_git rev-parse --show-toplevel 2>/dev/null)/.pre-commit-config.yaml
    test -r "$config" && string match -rg "\s+$key:\s+(\S+)" <$config | string unescape
end

# Global options
complete pre-commit -f
complete pre-commit -n __fish_use_subcommand -s h -l help -d "Show help message and exit"
complete pre-commit -n __fish_use_subcommand -s V -l version -d "Show version number and exit"
complete pre-commit -n "__fish_seen_subcommand_from $commands" -l color -xa "auto always never" -d "Whether to use color in output"
complete pre-commit -n "__fish_seen_subcommand_from $commands" -s c -l config -r -d "Path to alternate config file"
complete pre-commit -n "__fish_seen_subcommand_from $commands" -s h -l help -d "Show help message and exit"

# autoupdate
complete pre-commit -n __fish_use_subcommand -a autoupdate -d "Auto-update config"
complete pre-commit -n "__fish_seen_subcommand_from autoupdate" -l bleeding-edge -d "Update to the bleeding edge of `HEAD`"
complete pre-commit -n "__fish_seen_subcommand_from autoupdate" -l freeze -d "Store frozen hashes in `rev`"
complete pre-commit -n "__fish_seen_subcommand_from autoupdate" -l repo -xa "(__fish_pre_commit_config_print repo)" -d "Only update this repository"

# clean
complete pre-commit -n __fish_use_subcommand -a clean -d "Clean out files"

# gc
complete pre-commit -n __fish_use_subcommand -a gc -d "Clean unused cached repos"

# help
complete pre-commit -n __fish_use_subcommand -a help -d "Show help for a specific command"
complete pre-commit -n "__fish_seen_subcommand_from help" -a "$commands" -d Command

# init-templatedir
complete pre-commit -n __fish_use_subcommand -a init-templatedir -d "Install hook script in a directory"
complete pre-commit -n "__fish_seen_subcommand_from init-templatedir" -l no-allow-missing-config -d "Assume cloned repos should have a config"
complete pre-commit -n "__fish_seen_subcommand_from init-templatedir" -s t -l hook-type -xa "$hook_types" -d "Type of hook to install"
complete pre-commit -n "__fish_seen_subcommand_from init-templatedir" -a "(__fish_complete_directories)" -d "Directory to write scripts"

# install
complete pre-commit -n __fish_use_subcommand -a install -d "Install the pre-commit script"
complete pre-commit -n "__fish_seen_subcommand_from install" -l allow-missing-config -d "Allow missing config file"
complete pre-commit -n "__fish_seen_subcommand_from install" -l install-hooks -d "Install hook environments"
complete pre-commit -n "__fish_seen_subcommand_from install" -s t -l hook-type -xa "$hook_types" -d "Type of hook to install"
complete pre-commit -n "__fish_seen_subcommand_from install" -s f -l overwrite -d "Overwrite existing hooks"

# install-hooks
complete pre-commit -n __fish_use_subcommand -a install-hooks -d "Install all hook environments"

# migrate-config
complete pre-commit -n __fish_use_subcommand -a migrate-config -d "Migrate to newer config"

# run / try-repo
complete pre-commit -n __fish_use_subcommand -a run -d "Run hooks"
complete pre-commit -n __fish_use_subcommand -a try-repo -d "Try the hooks in a repository"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -l checkout-type -xa "0 1" -d "Indicate either branch or file checkout"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -l commit-msg-filename -r -d "Filename to check when running during `commit-msg`"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -l commit-object-name -d "Commit object name"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -l files -r -d "Filenames to run hooks on"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -l hook-stage -xa "$hook_stages" -d "The stage during which the hook is fired"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -l is-squash-merge -d "Whether the merge was a squash merge"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -l local-branch -d "Local branch ref used by `git push`"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -l prepare-commit-message-source -d "Source of the commit message"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -l remote-branch -d "Remote branch ref used by `git push`"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -l remote-name -d "Remote name used by `git push`"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -l remote-url -d "Remote url used by `git push`"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -l rewrite-command -d "The command that invoked the rewrite"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -l show-diff-on-failure -d "Show `git diff` on hook failure"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -s a -l all-files -d "Run on all the files in the repo"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -s s -l source -l from-ref -xa '(__fish_git_refs)' -d "Original ref"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -s o -l origin -l to-ref -xa '(__fish_git_refs)' -d "Destination ref"
complete pre-commit -n "__fish_seen_subcommand_from run try-repo" -s v -l verbose -d Verbose
complete pre-commit -n "__fish_seen_subcommand_from run" -a "(__fish_pre_commit_config_print id)" -d "Hook id"
complete pre-commit -n "__fish_seen_subcommand_from try-repo" -l ref -l rev -xa '(__fish_git_refs)' -d "Ref to run against"

# sample-config
complete pre-commit -n __fish_use_subcommand -a sample-config -d "Produce sample config file"

# uninstall
complete pre-commit -n __fish_use_subcommand -a uninstall -d "Uninstall the pre-commit script"
complete pre-commit -n "__fish_seen_subcommand_from uninstall" -s t -l hook-type -xa "$hook_types" -d "Type of hook to uninstall"

# validate-config
complete pre-commit -n __fish_use_subcommand -a validate-config -d "Validate .pre-commit-config.yaml files"
complete pre-commit -n "__fish_seen_subcommand_from validate-config" --force-files

# validate-manifest
complete pre-commit -n __fish_use_subcommand -a validate-manifest -d "Validate .pre-commit-hooks.yaml files"
complete pre-commit -n "__fish_seen_subcommand_from validate-manifest" --force-files
