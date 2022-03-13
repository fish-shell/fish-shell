# The following lists are complete as of FreeBSD 12

set -l commands aam apm attrib cmd debug defects devlist eject epc format fwdownload help hpa identify idle inquiry load modepage negotiate opcodes periphlist persist readcap reportluns reprobe rescan reset sanitize security sleep smpcmd smpmaninfo smppc smpphylist smprg standby start stop tags timestamp tur zone

complete -c camcontrol -n "__fish_is_nth_token 1" -xa "$commands"
