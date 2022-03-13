# dscacheutil
complete -c dscacheutil -f -d 'Directory Service cache utility'
complete -c dscacheutil -s h -d 'lists options' -f -n '[ (commandline -opc | count) -le 1 ]'
complete -c dscacheutil -s q -d 'initiate query' -f -x -n '[ (commandline -opc | count) -le 1 ] || contains -- -a (commandline -opc) || contains -- -q (commandline -opc) && [ (commandline -opc | count) -lt 3 ]' -a "
group\t'name or gid'
host\t'name or ip address'
mount\t'name'
protocol\t'name or number'
rpc\t'name or number'
service\t'name or port'
user\t'name or uid'
"
complete -c dscacheutil -s a -d '-q: specific key & value' -f -n 'contains -- -q (commandline -opc)'
complete -c dscacheutil -o cachedump -d 'dump cache overview' -f -n '[ (commandline -opc | count) -le 1 ]'
complete -c dscacheutil -o buckets -d 'show hash buckets' -f -n 'contains -- -cachedump (commandline -opc)'
complete -c dscacheutil -o entries -d '-cachedump: cache entries' -f -n 'contains -- -cachedump (commandline -opc)' -a "
group\t'name or gid'
host\t'name or ip address'
mount\t'name'
protocol\t'name or number'
rpc\t'name or number'
service\t'name or port'
user\t'name or uid'
"
complete -c dscacheutil -o configuration -d 'print current config' -f -n '[ (commandline -opc | count) -le 1 ]'
complete -c dscacheutil -o statistics -d 'prints cache stats' -f -n '[ (commandline -opc | count) -le 1 ]'
complete -c dscacheutil -o flushcache -d 'reset cache (DNS	)' -f -k -n '[ (commandline -opc | count) -le 1 ]'
