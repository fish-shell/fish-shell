#RUN: fish=%fish %fish -c '$fish -c false; echo RC: $status'
# CHECK: RC: 1
