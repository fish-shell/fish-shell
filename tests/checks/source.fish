#RUN: %fish %s

source /dev/null
echo $status
# CHECK: 0

source /
echo $status
# CHECKERR: error: Unable to read input file: Is a directory
# CHECKERR: source: Error while reading file '/'
# CHECK: 1

source unknown-file
# CHECKERR: source: Error encountered while sourcing file 'unknown-file':
# CHECKERR: source: {{.+}}

source <&-
# CHECKERR: source: stdin is closed
source - <&-
# CHECKERR: source: stdin is closed
