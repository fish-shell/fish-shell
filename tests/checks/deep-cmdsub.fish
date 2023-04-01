# RUN: %ghoti %s

# Ensure we don't hang on deep command substitutions - see #6503.

set s "echo hooray"
for i in (seq 63)
    set s "echo ($s)"
end
eval $s
#CHECK: hooray

