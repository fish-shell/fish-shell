
# magic completion safety check (do not remove this comment)
if not type -q composer.phar
    exit
end
complete -c composer.phar --wraps=composer
