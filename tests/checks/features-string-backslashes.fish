#RUN: %ghoti --features 'regex-easyesc' -C 'string replace -ra "\\\\" "\\\\\\\\" -- "a\b\c"'
# CHECK: a\\b\\c
