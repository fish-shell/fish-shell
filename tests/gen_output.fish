#!/usr/bin/env fish

#
# Generate output for a test script
#
# It is important that you verify that the generated
# output is correct!
#

for i in $argv
    set template_out (basename $i .in).out
    set template_err (basename $i .in).err
    set template_status (basename $i .in).status

    fish <$i >$template_out 2>$template_err
    echo $status >$template_status
end
