function fish_vcs_prompt --description "Print the prompts for all available vcsen"
    # If a prompt succeeded, we assume that it's printed the correct info.
    # This is so we don't try svn if git already worked.
    fish_git_prompt
    or fish_hg_prompt
    or fish_svn_prompt
end
