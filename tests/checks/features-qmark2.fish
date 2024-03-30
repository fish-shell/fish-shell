#RUN: %fish --features 'qmark-noglob' -C 'string match --quiet "??" ab ; echo "qmarkoff: $status"' | %filter-ctrlseqs
# CHECK: qmarkoff: 1
