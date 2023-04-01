# RUN: env PATH="a::b" CDPATH="d::e" MANPATH="x::y" %ghoti %s

# In PATH and CDPATH, empty elements are treated the same as "."
# In ghoti we replace them explicitly. Ensure that works.
# Do not replace empties in MATHPATH - see #4158.

echo "$PATH"
# CHECK: a:.:b

echo "$CDPATH"
# CHECK: d:.:e

echo "$MANPATH"
# CHECK: x::y

set PATH abc '' def
echo "$PATH"
# CHECK: abc:.:def

set CDPATH '' qqq
echo "$CDPATH"
# CHECK: .:qqq

set MANPATH 123 '' 456
echo "$MANPATH"
# CHECK: 123::456
