# This is needed because GNU and BSD implementations of `od` differ in the whitespace they emit.
# In the past we used the `xxd` command which doesn't suffer from such idiosyncrasies but it isn't
# available on some systems unless you install the Vim editor. Whereas `od` is pretty much
# universally available. See issue #3797.
#
# We use the lowest common denominator format, `-b`, because it should work in all implementations.
# I wish we could use the `-t` flag but it isn't available in every OS we're likely to run on.
#
function display_bytes
    od -b | sed -e 's/  */ /g' -e 's/  *$//'
end
