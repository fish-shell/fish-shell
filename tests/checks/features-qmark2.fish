#RUN: %ghoti --features 'qmark-noglob' -C 'string match --quiet "??" ab ; echo "qmarkoff: $status"'
# CHECK: qmarkoff: 1
