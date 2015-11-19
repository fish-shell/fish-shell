#
# Completion for sudo
#

function __fish_sudo_needs_command
	__fish_needs_subcommand --flagshort {A,b,E,e,g,H,i,K,k,l,n,P,S,s} --flag {askpass,background,preserve-env,edit,set-home,login,remove-timestamp,reset-timestamp,list,non-interactive,preserve-groups,stdin,shell} --argshort {C,g,h,p,U,u} --arg {close-from,group,host,prompt,other-user,user} --exit help version validate --exitshort h v V
end

complete -c sudo -s h -n "__fish_no_arguments" --description "Display help and exit" -f
complete -c sudo -s v -n "__fish_no_arguments" --description "Validate" -f
# Only offer sudo options if no command has been given
complete -c sudo -s u -a "(__fish_complete_users)" -x -d "Run command as user" -n "__fish_sudo_needs_command" -f
complete -c sudo -s g -a "(__fish_complete_groups)" -x -d "Run command as group" -n "__fish_sudo_needs_command" -f
complete -c sudo -s E -d "Preserve environment" -n "__fish_sudo_needs_command" -f
complete -c sudo -s P -d "Preserve group vector" -n "__fish_sudo_needs_command" -f
complete -c sudo -s S -d "Read password from stdin" -n "__fish_sudo_needs_command" -f
complete -c sudo -s H -d "Set home" -n "__fish_sudo_needs_command" -f
complete -c sudo -s e -r -d "Edit" -n "__fish_sudo_needs_command"
# Remove the description once we have a command
complete -c sudo --description "Command to run" -x -a "(__fish_complete_subcommand_root -u -g)" -n "__fish_sudo_needs_command" -f
complete -c sudo -x -a "(__fish_complete_subcommand_root -u -g)" -n "not __fish_sudo_needs_command" -f

# Since sudo runs subcommands, it can accept any switches
complete -c sudo -u
