# RUN: %fish %s

# Load the source-tree function explicitly so this test can exercise the hosts-file filter
# without depending on the machine's real /etc/hosts.
set -l workspace_root (path resolve -- (status dirname)/../../)
source $workspace_root/share/functions/__fish_print_hostnames.fish

__fish_print_hostnames_from_hosts \
    '127.0.0.1 localhost' \
    '0.0.0.0 blocked' \
    '192.0.2.1 example.test alias.test'
# CHECK: example.test
# CHECK: alias.test
