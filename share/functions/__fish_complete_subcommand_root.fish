

function __fish_complete_subcommand_root -d "Run the __fish_complete_subcommand function using a PATH containing /sbin and /usr/sbin"
	set -l PATH_OLD $PATH 
	set PATH /sbin /usr/sbin $PATH
	__fish_complete_subcommand
	set PATH $PATH_OLD
end
