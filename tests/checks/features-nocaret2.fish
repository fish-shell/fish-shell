#RUN: %ghoti --features    'stderr-nocaret' -c 'status test-feature stderr-nocaret; echo nocaret: $status'
# CHECK: nocaret: 0
