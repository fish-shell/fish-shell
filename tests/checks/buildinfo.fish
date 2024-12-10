#RUN: %fish %s
# THIS WILL FAIL ON CI
# There is no real good way to match this other than
# Version: {{.*}}
# and so on (with some exceptions like build system being "Cargo" or "CMake")
status buildinfo
