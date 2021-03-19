#RUN: %fish --features regex-easyesc %s
string replace -r 'a(.*)' '\U$0\E' abc
# CHECK: ABC
