#RUN: %fish --features=remove-percent-self %s

echo %self
# CHECK: %self
