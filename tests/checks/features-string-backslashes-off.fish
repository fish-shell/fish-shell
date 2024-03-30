#RUN: %fish --features 'no-regex-easyesc' -C 'string replace -ra "\\\\" "\\\\\\\\" -- "a\b\c"' | %filter-ctrlseqs
# CHECK: a\b\c
