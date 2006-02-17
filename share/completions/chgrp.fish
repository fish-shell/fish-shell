
complete -c chgrp -s c -l changes -d (_ "Output diagnostic for changed files")
complete -c chgrp -l dereference -d (_ "Dereferense symbolic links")
complete -c chgrp -s h -l no-dereference -d (_ "Do not dereference symbolic links")
complete -c chgrp -l from -d (_ "Change from owner/group")
complete -c chgrp -s f -l silent -d (_ "Supress errors")
complete -c chgrp -l reference -d (_ "Use same owner/group as file") -r
complete -c chgrp -s R -l recursive -d (_ "Operate recursively")
complete -c chgrp -s v -l verbose -d (_ "Output diagnostic for every file")
complete -c chgrp -s h -l help -d (_ "Display help and exit")
complete -c chgrp -l version -d (_ "Display version and exit")
complete -c chgrp -d Group -a "(__fish_complete_groups)"
