#RUN: %fish --features=remove-percent-self -C 'set -g fish_indent %fish_indent' %s

echo %self
# CHECK: %self
