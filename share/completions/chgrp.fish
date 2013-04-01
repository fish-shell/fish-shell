
complete -c chgrp -s c -l changes --description "Output diagnostic for changed files"
complete -c chgrp -l dereference --description "Dereference symbolic links"
complete -c chgrp -s h -l no-dereference --description "Do not dereference symbolic links"
complete -c chgrp -l from --description "Change from owner/group"
complete -c chgrp -s f -l silent --description "Suppress errors"
complete -c chgrp -l reference --description "Use same owner/group as file" -r
complete -c chgrp -s R -l recursive --description "Operate recursively"
complete -c chgrp -s v -l verbose --description "Output diagnostic for every file"
complete -c chgrp -s h -l help --description "Display help and exit"
complete -c chgrp -l version --description "Display version and exit"
complete -c chgrp -d Group -a "(__fish_complete_groups)"
