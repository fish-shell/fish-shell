# Explicitly overriding HOME/XDG_CONFIG_HOME is only required if not invoking via `make test`
# RUN: env HOME="$(mktemp -d)" XDG_CONFIG_HOME="$(mktemp -d)" %fish --features '' -c 'string match --quiet "??" ab ; echo "qmarkon: $status"'
#CHECK: qmarkon: 0
