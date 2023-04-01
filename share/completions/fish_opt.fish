set --local CONDITION '! __ghoti_seen_argument --short r --long required-val --short o --long optional-val'

complete --command ghoti_opt --no-files

complete --command ghoti_opt --short-option h --long-option help --description 'Show help'

complete --command ghoti_opt --short-option s --long-option short --no-files --require-parameter --description 'Specify short option'
complete --command ghoti_opt --short-option l --long-option long --no-files --require-parameter --description 'Specify long option'
complete --command ghoti_opt --long-option longonly --description 'Use only long option'
complete --command ghoti_opt --short-option o --long-option optional-val -n $CONDITION --description 'Don\'t require value'
complete --command ghoti_opt --short-option r --long-option required-val -n $CONDITION --description 'Require value'
complete --command ghoti_opt --long-option multiple-vals --description 'Store all values'
