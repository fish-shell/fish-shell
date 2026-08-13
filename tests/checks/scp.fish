#RUN: fish=%fish %fish %s

# scp treats a token as a remote target only when a colon appears before any
# slash (matching OpenSSH). A local path whose colon follows a slash must not
# be parsed as a remote host, otherwise completion shells out to scp/ssh with a
# bogus host and prints an error.

for token in ./dir/file-1:106-any.pkg a/b:c host:path user@host:/etc
    string match -rq -- '^[^/]*:' $token; and echo remote; or echo local
end
# CHECK: local
# CHECK: local
# CHECK: remote
# CHECK: remote
