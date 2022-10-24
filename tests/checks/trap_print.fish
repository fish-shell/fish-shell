# RUN: env fth=%fish_test_helper %fish %s

# These tests check how installed trap handlers are listed. Run separately from the main trap
# handler to ensure a clean environment.

trap -p
# CHECK:

# Validate both SIG, no-SIG, uppercase, and lowercase.
trap "true" SIGTERM kill ExIT INT

trap -p
# CHECK: # Defined via `source`
# CHECK: function __trap_handler_EXIT --on-event fish_exit
# CHECK:  true;
# CHECK: end
# CHECK: # Defined via `source`
# CHECK: function __trap_handler_INT --on-signal SIGINT
# CHECK:  true;
# CHECK: end
# CHECK: # Defined via `source`
# CHECK: function __trap_handler_KILL --on-signal SIGKILL
# CHECK:  true;
# CHECK: end
# CHECK: # Defined via `source`
# CHECK: function __trap_handler_TERM --on-signal SIGTERM
# CHECK:  true;
# CHECK: end
