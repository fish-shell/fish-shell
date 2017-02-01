
function __fish_complete_file_url
    set -l comp

    if set -q argv[1]
        set comp $argv[1]
    else
        set comp (commandline -ct)
    end

    set -l prefix (echo $comp|cut -c 1-7)

    if test file:// = $prefix
        set -l stripped (echo $comp|cut -c 8-)
        printf "%s\n" file://(complete -C"echo $stripped")
    else
        echo file://
    end

end
