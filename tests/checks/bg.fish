# RUN: %fish %s

bg 0
# CHECKERR: bg: '0' is not a valid process ID
bg -- -1
# CHECKERR: bg: '-1' is not a valid process ID
bg -- -(math 2 ^ 31)
# CHECKERR: bg: '-2147483648' is not a valid process ID
