function fish_vcs_prompt --description "Print all vcs prompts"
    # If a prompt succeeded, we assume that it's printed the correct info.
    # This is so we don't try svn if git already worked.
    fish_jj_prompt $argv
    or fish_git_prompt $argv
    or fish_hg_prompt $argv
    # The svn and fossil prompts are disabled by default because they can be quite slow.
    # To enable them uncomment them.
    # You can also only use them in specific directories by checking $PWD.
    # or fish_fossil_prompt $argv
    # or fish_svn_prompt
end
