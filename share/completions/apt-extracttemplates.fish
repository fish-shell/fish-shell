
# magic completion safety check (do not remove this comment)
if not type -q apt-extracttemplates
    exit
end

#apt-extracttemplates
complete -c apt-extracttemplates -s h -l help -d "Display help and exit"
complete -r -c apt-extracttemplates -s t -d "Set temp dir"
complete -r -c apt-extracttemplates -s c -d "Specifiy config file"
complete -r -c apt-extracttemplates -s o -d "Specify options"

