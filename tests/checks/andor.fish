#RUN: %fish %s

set -xl LANG C # uniform quotes

eval 'true | and'
# CHECKERR: {{.*}}: The 'and' command can not be used in a pipeline

eval 'true | or'
# CHECKERR: {{.*}}: The 'or' command can not be used in a pipeline
