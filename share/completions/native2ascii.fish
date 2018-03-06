# native2ascii

# magic completion safety check (do not remove this comment)
if not type -q native2ascii
    exit
end
complete -c native2ascii -o reverse -d 'Perform the reverse operation'
complete -c native2ascii -o encoding -d 'Specifies the name of the character encoding'
complete -c native2ascii -o Joption -d 'Pass "option" to JVM'
