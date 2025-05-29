#RUN: fish=%fish %fish %s

$fish -c '
    complete -e cat
    complete -C"cat -" | wc -l
'
# CHECK: 0

# Note: this is *not* handled by the tombstone logic.
# We don't reload cat.fish because it is cached by the autoloader,
# and we don't invalidate the cache.
$fish -c '
    complete -C"cat -" >/dev/null
    complete -e cat
    complete -C"cat -" | wc -l
'
# CHECK: 0
