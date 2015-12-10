function __fish_vcs_prompt --description "Print the prompts for all available vcsen"
	__fish_git_prompt
	__fish_hg_prompt
	__fish_svn_prompt
end
