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

# Include, read and expanded like ssh does, without eval.
mkdir -p ~/.ssh/conf.d ~/.ssh/extra.d ~/'.ssh/with space'
echo 'Host inc_quoted_a' >~/.ssh/conf.d/a.conf
echo 'Host inc_quoted_b' >~/.ssh/conf.d/b.conf
echo 'Host inc_hidden' >~/.ssh/conf.d/.hidden.conf
echo 'Host inc_multi_a' >~/.ssh/multi-a
echo 'Host inc_multi_b' >~/.ssh/multi-b
echo 'Host inc_equals' >~/.ssh/equals
echo 'Host inc_qmark' >~/.ssh/extra.d/x-1.conf
echo 'Host inc_single' >~/'.ssh/with space/single'
echo 'Host inc_escaped' >~/'.ssh/with space/escaped'
echo 'Host inc_env' >~/.ssh/env
echo 'Host inc_unset_env' >~/.ssh/unset-env
set -gx __fish_test_ssh_dir ~/.ssh
set -e __fish_test_unset
printf '%s\n' \
    'Include "conf.d/*.conf"' \
    'Include multi-a ~/.ssh/multi-b # a comment' \
    'Include=equals' \
    'Include extra.d/x-?.conf' \
    "Include 'with space/single' with\\ space/escaped" \
    'Include ${__fish_test_ssh_dir}/env ${__fish_test_unset}/unset-env' \
    'Include (touch include_ran) $(touch include_ran)' \
    'Include "unterminated' >~/.ssh/config
__fish_print_hostnames | string match 'inc_*' | sort
# CHECK: inc_env
# CHECK: inc_equals
# CHECK: inc_escaped
# CHECK: inc_multi_a
# CHECK: inc_multi_b
# CHECK: inc_qmark
# CHECK: inc_quoted_a
# CHECK: inc_quoted_b
# CHECK: inc_single
path is include_ran
or echo not run
# CHECK: not run

# Host and KnownHostsFile, read like ssh does.
echo 'kh_custom ssh-ed25519 AAAA' >~/'.ssh/known hosts'
echo 'kh_env ssh-ed25519 AAAA' >~/.ssh/known_hosts_env
printf '%s\n' \
    'Host host_plain host_* !host_negated host_two # host_comment' \
    'Host="host_quoted" \'host_single\' mid"dl"e "#host_not_a_comment" "with space"' \
    'Host "host_unterminated' \
    '# UserKnownHostsFile ~/.ssh/commented' \
    'UserKnownHostsFile "~/.ssh/known hosts"' \
    'GlobalKnownHostsFile=${__fish_test_ssh_dir}/known_hosts_env' >~/.ssh/config
__fish_print_hostnames | string match -r '^(?:host_|mid|#host|kh_|with).*' | sort
# CHECK: #host_not_a_comment
# CHECK: host_plain
# CHECK: host_quoted
# CHECK: host_single
# CHECK: host_two
# CHECK: kh_custom
# CHECK: kh_env
# CHECK: middle
