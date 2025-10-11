# RUN: %fish %s

disown 0
# CHECKERR: disown: '0' is not a valid process ID
disown -- -1
# CHECKERR: disown: '-1' is not a valid process ID
disown -- -(math 2 ^ 31)
# CHECKERR: disown: '-2147483648' is not a valid process ID
