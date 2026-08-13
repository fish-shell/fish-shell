#RUN: fish=%fish %fish %s

# scp treats a token as a remote target only when a colon appears before any
# slash (matching OpenSSH). A local path whose colon follows a slash must not
# be parsed as a remote host, otherwise completion shells out to scp/ssh with a
# bogus host and prints an error.

source $__fish_data_dir/completions/scp.fish

__scp_looks_remote ./dir/file-1:106-any.pkg; and echo remote; or echo local
# CHECK: local

__scp_looks_remote a/b:c; and echo remote; or echo local
# CHECK: local

__scp_looks_remote host:path; and echo remote; or echo local
# CHECK: remote

__scp_looks_remote user@host:/etc; and echo remote; or echo local
# CHECK: remote

echo (__scp_remote_target user@host:/etc):(__scp_remote_path_prefix user@host:/etc)
# CHECK: user@host:/etc
