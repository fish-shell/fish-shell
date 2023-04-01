function ghoti_vcs_prompt --description "Print all vcs prompts"
    # If a prompt succeeded, we assume that it's printed the correct info.
    # This is so we don't try svn if git already worked.
    ghoti_git_prompt $argv
    or ghoti_hg_prompt $argv
    or ghoti_fossil_prompt $argv
    # The svn prompt is disabled by default because it's quite slow on common svn repositories.
    # To enable it uncomment it.
    # You can also only use it in specific directories by checking $PWD.
    # or ghoti_svn_prompt
end
