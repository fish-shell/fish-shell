#RUN: %fish --features    'remove-percent-self' -c 'status test-feature remove-percent-self; echo remove-percent-self: $status'
# CHECK: remove-percent-self: 0
