
complete -c chgrp -s c -l changes -d (N_ "Output diagnostic for changed files")
complete -c chgrp -l dereference -d (N_ "Dereferense symbolic links")
complete -c chgrp -s h -l no-dereference -d (N_ "Do not dereference symbolic links")
complete -c chgrp -l from -d (N_ "Change from owner/group")
complete -c chgrp -s f -l silent -d (N_ "Supress errors")
complete -c chgrp -l reference -d (N_ "Use same owner/group as file") -r
complete -c chgrp -s R -l recursive -d (N_ "Operate recursively")
complete -c chgrp -s v -l verbose -d (N_ "Output diagnostic for every file")
complete -c chgrp -s h -l help -d (N_ "Display help and exit")
complete -c chgrp -l version -d (N_ "Display version and exit")
complete -c chgrp -d Group -a "(__fish_complete_groups)"
