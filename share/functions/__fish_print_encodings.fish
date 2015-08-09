function __fish_print_encodings -d "Complete using available character encodings"
    iconv --list | sed -e 's|//||'
end
