function funcdel -a funcname
    functions -e $funcname
    rm $fish_function_path/$funcname.fish
end
