set -l __fish_iptables_tables filter nat mangle raw security

function __fish_iptables_current_table
	set -l next_is_table 1
	for token in (commandline -oc)
		switch $token
			case "--table=*"
				set -l IFS "="
				echo $token | while read a b
					echo $b
				end
				return 0
			case "--table"
				set next_is_table 0
			case "-*t*"
				set next_is_table 0
			case "*"
				if [ $next_is_table -eq 0 ]
					echo $token
					return 0
				end
		end
	end
	return 1
end

function __fish_iptables_user_chains
	# There can be user-defined chains so we need iptables' help
	set -l tablearg
	set -l table (__fish_iptables_current_table)
	if __fish_iptables_current_table
		set tablearg "--table=$table"
	end
	# This only works as root, so ignore errors
	iptables $tablearg -L ^/dev/null | grep Chain | while read a b c
		echo $b
	end
end


function __fish_iptables_chains
	set -l table (__fish_iptables_current_table)
	[ -z $table ]; and set -l table "*"
	set -l prerouting "PREROUTING	For packets that are coming in"
	set -l input "INPUT	For packets destined to local sockets"
	set -l output "OUTPUT	For locally-generated packets"
	set -l forward "FORWARD	For packets being routed through"
	set -l postrouting "POSTROUTING	For packets that are about to go out"
	switch $table
		case "filter"
			echo $input
			echo $forward
			echo $output
		case "nat"
			echo $prerouting
			echo $output
			echo $postrouting
		case "mangle"
			echo $prerouting
			echo $input
			echo $output
			echo $forward
			echo $postrouting
		case "raw"
			echo $prerouting
			echo $output
		case "security"
			echo $input
			echo $output
			echo $forward
		case '*'
			echo $prerouting
			echo $input
			echo $output
			echo $forward
			echo $postrouting
	end
	__fish_iptables_user_chains
end

function __fish_iptables_has_chain
	# Remove descriptions
	set -l IFS "	"
	set -l chains (__fish_iptables_chains | while read a b; echo $a; end)
	set -e IFS
	set -l cmdline (commandline -op)
	for c in $chains
		if contains -- $c $cmdline
			return 0
		end
	end
	return 1
end

# A target is a user-defined chain, one of "ACCEPT DROP RETURN" or an extension (TODO)
function __fish_iptables_targets
	echo "ACCEPT"
	echo "DROP"
	echo "RETURN"
	__fish_iptables_chains
end

### Commands
complete -c iptables -s A -l append --description 'Append rules to the end of a chain' -a '(__fish_iptables_chains)' -f
complete -c iptables -s C -l check --description 'Check whether a matching rule exists in a chain' -a '(__fish_iptables_chains)' -f
complete -c iptables -s D -l delete --description 'Delete rules from a chain' -a '(__fish_iptables_chains)' -f
# Rulespec is match (can't complete that) and then a target
# TODO: This is only valid for some options, as others need a rulenum first or no rulespec at all
complete -c iptables -n '__fish_contains_opt -s A -s C -s D append check delete; and __fish_iptables_has_chain' -s j -l jump --description 'Specify the target of a rule' -f -a\
'(__fish_iptables_targets)'

complete -c iptables -n '__fish_contains_opt -s A -s C -s D append check delete; and __fish_iptables_has_chain' -s m -l match --description 'Specify a match to use' -f
complete -c iptables -n '__fish_iptables_has_chain' -a 'ACCEPT DROP RETURN' -f
complete -c iptables -n '__fish_iptables_has_chain' -a '( __fish_iptables_user_chains)' -f
complete -c iptables -s I -l insert --description 'Insert rules in the beginning of a chain' -a '(__fish_iptables_chains)' -f
complete -c iptables -s R -l replace --description 'Replace a rule in a chain' -a '(__fish_iptables_chains)' -f
complete -c iptables -s L -l list --description 'List all rules in a chain' -a '(__fish_iptables_chains)' -f
complete -c iptables -s S -l list-rules --description 'Print all rules in a chain.' -a '(__fish_iptables_chains)' -f
complete -c iptables -s F -l flush --description 'Delete ALL rules in a chain' -a '(__fish_iptables_chains)' -f
complete -c iptables -s Z -l zero --description 'Zero the packet and byte counters in chains' -a '(__fish_iptables_chains)' -f
complete -c iptables -s N -l new-chain --description 'Create a new user-defined chain by the given name' -a '(__fish_iptables_chains)' -f
complete -c iptables -s X -l delete-chain --description 'Delete the optional user-defined chain specified' -a '(__fish_iptables_chains)' -f
complete -c iptables -s P -l policy --description 'Set the policy for the chain to the given target' -a '(__fish_iptables_chains)' -f
complete -c iptables -s E -l rename-chain --description 'Rename the user specified chain to the user supplied name' -a '(__fish_iptables_chains)' -f
complete -c iptables -s h --description 'Help' -f

complete -c iptables -s p -l protocol --description 'The protocol of the rule or of the packet to check' -f
complete -c iptables -s s -l source --description 'Source specification' -f
complete -c iptables -s d -l destination --description 'Destination specification' -f
complete -c iptables -s j -l jump --description 'Specify the target of a rule' -f
complete -c iptables -s i -l in-interface --description 'Interface via which a packet was received' -f
complete -c iptables -s o -l out-interface --description 'Interface via which packet is to be sent' -f
complete -c iptables -s f -l fragment --description 'Rule only refers to second and further ipv4 fragments' -f
complete -c iptables -s c -l set-counters --description 'Initialize packet and byte counters of a rule' -f
complete -c iptables -s v -l verbose --description 'Verbose output' -f
complete -c iptables -s w -l wait --description 'Wait for the xtables lock' -f
complete -c iptables -s n -l numeric --description 'Numeric output' -f
complete -c iptables -s x -l exact --description 'Expand numbers' -f
complete -c iptables -l line-numbers --description 'When listing rules, add line numbers' -f
complete -c iptables -s t -l table --description 'The table to operate on' -a "$__fish_iptables_tables" -f

# Options that take files
complete -c iptables -l modprobe --description 'Use this command to load modules' -r

# I don't get these
# complete -c iptables -s 4 -l ipv4 --description 'This option has no effect in iptables and iptables-restore.' -f
# complete -c iptables -s 6 -l ipv6 --description 'If a rule using the -6 option is inserted with … [See Man Page]' -f
# complete -c iptables -s g -l goto --description '' -f

# Should aliased options be in the completion?
# complete -c iptables -l dst --description 'Alias for -d' -f
