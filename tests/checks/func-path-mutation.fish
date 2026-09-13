# RUN: %fish %s
# A function that mutates $fish_function_path (even restoring it before returning)
# must not break resolution of an autoloaded function in a later pipeline element.
fish_test_modpath | fish_test_constant
# CHECK: Hello, Constant World!
