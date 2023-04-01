#RUN: %ghoti --features 'no-stderr-nocaret' -c 'echo -n careton:; echo ^/dev/null'
# CHECK: careton:^/dev/null
