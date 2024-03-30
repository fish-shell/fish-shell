#!/bin/sh

escape=$(printf '\033')

"$1" -c 'cat | string replace -ra $argv[1] ""' -- "\
""$escape\[\?2004h""\
""$escape\[>4;1m""\
""$escape\[>5u""\
""$escape=""\
""($escape\[\?1004h)?""\
""|""\
""$escape\[\?2004l""\
""$escape\[>4;0m""\
""$escape\[<1u""\
""$escape>""\
""$escape\[\?1004l"
