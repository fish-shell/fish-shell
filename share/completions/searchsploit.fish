# https://www.kali.org/tools/exploitdb/#searchsploit
# https://www.exploit-db.com/searchsploit

complete -c searchsploit -f -d "Search Exploit-DB"

complete -c searchsploit -s c -l case -d "Case-sensitive search"
complete -c searchsploit -s e -l exact -d "Exact match on exploit title"
complete -c searchsploit -s h -l help -d "Show help message"
complete -c searchsploit -s j -l json -d "Show result in json format"
complete -c searchsploit -s m -l mirror -x -d "Mirror an exploit to current directory"
complete -c searchsploit -s o -l overflow -d "Exploit titles are allowed to overflow their columns"
complete -c searchsploit -s p -l path -d "Show the full path to an exploit"
complete -c searchsploit -s s -l strict -d "Perform a strict search"
complete -c searchsploit -s t -l title -d "Search just the exploit title"
complete -c searchsploit -s u -l update -d "Update exploit packages"
complete -c searchsploit -s v -l verbose -d "Display more information in output"
complete -c searchsploit -s w -l www -d "Show URL to Exploit-DB instead of local path"
complete -c searchsploit -s x -l examine -d "Examine (aka opens) the exploit using \$PAGER"

complete -c searchsploit -l disable-colour -d "Disable colour highlighting in search results"
complete -c searchsploit -l cve -d "Search with CVE value"
complete -c searchsploit -l exclude -x -d "Remove values from results"
complete -c searchsploit -l id -d "Display the EDB-ID value rather than local path"
complete -c searchsploit -l nmap -F -d "Checks results in Nmap's XML output"
