#RUN: %fish --features 'no-ignore-terminfo' -c 'status test-feature ignore-terminfo; echo ignore-terminfo: $status'
# CHECK: ignore-terminfo: 0
