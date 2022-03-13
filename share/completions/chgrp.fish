if chgrp --version &>/dev/null # not unix
    complete -c chgrp -s c -l changes -d "Output diagnostic for changed files"
    complete -c chgrp -l dereference -d "Dereference symlinks"
    complete -c chgrp -s h -l no-dereference -d "Don't dereference symlinks"
    complete -c chgrp -l from -d "Change from owner/group"
    complete -c chgrp -s f -l silent -d "Suppress errors"
    complete -c chgrp -l reference -d "Use same owner/group as file" -r
    complete -c chgrp -s R -l recursive -d "Operate recursively"
    complete -c chgrp -s v -l verbose -d "Output diagnostic for every file"
    complete -c chgrp -l help -d "Display help and exit"
    complete -c chgrp -l version -d "Display version and exit"
    complete -c chgrp -d Group -a "(__fish_complete_groups)"
else # not Linux
    complete -c chgrp -s h -d "Don't dereference symlinks"
    complete -c chgrp -s H -d "Follow specified symlinks with -R"
    complete -c chgrp -s L -d "Follow all symlinks with -R"
    complete -c chgrp -s P -d "Follow no symlinks with -R"
    complete -c chgrp -s f -d "Suppress errors"
    complete -c chgrp -s n -d "id is numeric; avoid lookup"
    complete -c chgrp -s R -d "Operate recursively"
    complete -c chgrp -s v -d "Output filenames"
    complete -c chgrp -s x -d "Don't traverse fs mountpoints"
    complete -c chgrp -d Group -a "(__fish_complete_groups)"
end
