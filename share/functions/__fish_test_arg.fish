
function __fish_test_arg --description "Test if the token under the cursor matches the specified wildcard"
    switch (commandline -ct)
        case $argv
            return 0
    end
    return 1
end

