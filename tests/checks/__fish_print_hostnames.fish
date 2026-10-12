# RUN: fish=%fish %fish %s
#
# Regression tests for __fish_print_hostnames' handling of ssh_config
# Include directives.

mkdir -p $HOME/.ssh/conf.d $HOME/.ssh/extra.d

# Quoted Include path containing a wildcard. The surrounding quotes are
# syntactic in ssh_config and must not suppress wildcard expansion.
echo 'Include "~/.ssh/conf.d/*.conf"' >$HOME/.ssh/config

# Multiple whitespace-separated pathnames on a single Include line must
# each be processed.
echo 'Include ~/.ssh/multi-a ~/.ssh/multi-b' >>$HOME/.ssh/config

# Multiple whitespace-separated pathnames combined with wildcards on the
# same Include line: every path must be split out and globbed.
echo 'Include ~/.ssh/extra.d/x-*.conf ~/.ssh/extra.d/y-*.conf' >>$HOME/.ssh/config

echo 'Host __fish_test_hostnames_quoted_b' >$HOME/.ssh/conf.d/b.conf
echo 'Host __fish_test_hostnames_quoted_a' >$HOME/.ssh/conf.d/a.conf
echo 'Host __fish_test_hostnames_multi_a'  >$HOME/.ssh/multi-a
echo 'Host __fish_test_hostnames_multi_b'  >$HOME/.ssh/multi-b
echo 'Host __fish_test_hostnames_glob_x1'  >$HOME/.ssh/extra.d/x-1.conf
echo 'Host __fish_test_hostnames_glob_x2'  >$HOME/.ssh/extra.d/x-2.conf
echo 'Host __fish_test_hostnames_glob_y1'  >$HOME/.ssh/extra.d/y-1.conf

__fish_print_hostnames | string match -e __fish_test_hostnames_ | sort -u
# CHECK: __fish_test_hostnames_glob_x1
# CHECK: __fish_test_hostnames_glob_x2
# CHECK: __fish_test_hostnames_glob_y1
# CHECK: __fish_test_hostnames_multi_a
# CHECK: __fish_test_hostnames_multi_b
# CHECK: __fish_test_hostnames_quoted_a
# CHECK: __fish_test_hostnames_quoted_b
