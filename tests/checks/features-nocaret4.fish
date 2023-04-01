#RUN: %ghoti --features '   stderr-nocaret' -c 'echo -n "caretoff: "; echo ^/dev/null'
# CHECK: caretoff: ^/dev/null
