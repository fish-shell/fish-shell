
complete -c chgrp -s c -l changes -d "Output diagnostic for changed files"
complete -c chgrp -l dereference -d "Dereference symbolic links"
complete -c chgrp -s h -l no-dereference -d "Do not dereference symbolic links"
complete -c chgrp -l from -d "Change from owner/group"
complete -c chgrp -s f -l silent -d "Suppress errors"
complete -c chgrp -l reference -d "Use same owner/group as file" -r
complete -c chgrp -s R -l recursive -d "Operate recursively"
complete -c chgrp -s v -l verbose -d "Output diagnostic for every file"
complete -c chgrp -s h -l help -d "Display help and exit"
complete -c chgrp -l version -d "Display version and exit"
complete -c chgrp -d Group -a "(__fish_complete_groups)"
