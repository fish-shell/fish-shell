# Explicitly overriding HOME/XDG_CONFIG_HOME is only required if not invoking via `make test`
# RUN: %fish --features '' -c 'string match --quiet "??" ab ; echo "qmarkon: $status"' | %filter-ctrlseqs
#CHECK: qmarkon: 1
