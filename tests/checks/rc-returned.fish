#RUN: %fish -c '%raw_fish -c false; echo RC: $status'
# CHECK: RC: 1
