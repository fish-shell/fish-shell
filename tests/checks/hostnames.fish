# RUN: %fish %s

# Load the source-tree function explicitly so this test can exercise the hosts-file filter
# without depending on the machine's real /etc/hosts.
set -l workspace_root (path resolve -- (status dirname)/../../)
source $workspace_root/share/functions/__fish_print_hostnames.fish

__fish_print_hostnames_from_hosts \
    '127.0.0.1 localhost' \
    '0.0.0.0 blocked' \
    '255.255.255.255 broadcasthost' \
    '192.0.2.1 example.test alias.test'
# CHECK: example.test
# CHECK: alias.test

# The ssh config of an ssh wrapper function's -F, expanded like the command line.
echo 'Host from_wrapper_config' >~/wrapper_config
function ssh
    command ssh -F ~/wrapper_config $argv
end
complete -C 'ssh ' | string match -q 'from_wrapper_config*'
and echo found
# CHECK: found
# A command substitution after -F is not run on <Tab>.
complete -C 'ssh -F (touch tab_ran) ' >/dev/null
path is tab_ran
or echo not run
# CHECK: not run
functions -e ssh
