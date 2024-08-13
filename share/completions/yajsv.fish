complete -c yajsv -s h -l help -d 'Show help'
complete -c yajsv -s v -d 'Show version'

complete -c yajsv -s b -r -d "Allow BOM in JSON files"
complete -c yajsv -s l -r -d "Validate the JSON documents from newline separated paths and/or globs in a text file"
complete -c yajsv -s q -r -d "Hide everything except errors"
complete -c yajsv -s r -r -d "The referenced schema"
complete -c yajsv -s s -r -d "The JSON schema to validate against"
