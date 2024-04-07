#RUN: %fish --features regex-easyesc %s | %filter-ctrlseqs
string replace -r 'a(.*)' '\U$0\E' abc
# CHECK: ABC
