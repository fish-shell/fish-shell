#RUN: env %fish --features '' -c 'string match --quiet "??" ab ; echo "qmarkon: $status"'
#CHECK: qmarkon: 0
