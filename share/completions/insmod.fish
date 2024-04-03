function _insmod_complete
    # List all .ko files in the current directory for completion.
    for file in (ls *.ko)
        echo $file
    end
end

complete -c insmod -a '(_insmod_complete)' -f

