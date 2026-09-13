function fish_test_modpath
    set --prepend -- fish_function_path /nonexistent
    set --erase -- fish_function_path[1]
    echo 'Hello, Mutated World!'
end
