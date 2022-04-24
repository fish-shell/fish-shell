function funcdel -a funcname

    set funcpath $fish_function_path/$funcname.fish

    if not type -q $funcname
        echo "$funcname is not defined as a function"
        return 1
    end

    if not test -e $funcpath
        echo "$funcname does not exist as a user function"
        return 2
    end

    functions -e $funcname
    rm $funcpath

end
