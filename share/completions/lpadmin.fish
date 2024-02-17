complete -c lpadmin -s c -d 'Adds the named printer to class' -x
complete -c lpadmin -s i -d 'Sets a  SysV style interface script for printer' -x
complete -c lpadmin -s m -d 'Sets a standard SysV interface script or PPD file ' -x
complete -c lpadmin -s R -d 'Deletes the named option from printer' -xa '(__fish_print_lpr_options)'
complete -c lpadmin -s r -d 'Removes the named printer from class' -x

complete -c lpadmin -s v -d 'Sets the device-uri attribute of the printer queue' -r
complete -c lpadmin -s D -d 'Provides a textual description of the destination' -x
complete -c lpadmin -s E -d 'Enables the destination and accepts jobs'
complete -c lpadmin -s L -d 'Provides a textual location of the destination' -x
complete -c lpadmin -s P -d 'Specify a PDD file to use with the printer' -k -xa "(__fish_complete_suffix .ppd .ppd.gz)"
complete -c lpadmin -s o -xa cupsIPPSupplies=true -d 'Specify if IPP supply level values should be reported'
complete -c lpadmin -s o -xa cupsIPPSupplies=false -d 'Specify if IPP supply level values should be reported'
complete -c lpadmin -s o -xa cupsSNMPSupplies=true -d 'Specify if SNMP supply level values should be reported'
complete -c lpadmin -s o -xa cupsSNMPSupplies=false -d 'Specify if SNMP supply level values should be reported'
complete -c lpadmin -s o -xa job-k-limit= -d 'Sets the kilobyte limit for per-user quotas'
complete -c lpadmin -s o -xa job-page-limit= -d 'Sets the page limit for per-user quotas (int) '
complete -c lpadmin -s o -xa job-quota-period= -d 'Sets the accounting period for per-user quotas (sec)'
complete -c lpadmin -s o -xa job-sheets-default= -d 'Sets the default banner page(s) to use for print jobs'
complete -c lpadmin -s o -d 'Sets a  PPD option for the printer' -xa '(__fish_complete_lpr_option)'
#complete -c lpadmin -s o -d 'Sets a default server-side option for the destination' -xa '(__fish_complete_lpr_option | sed "s/=/-default=/")'
complete -c lpadmin -s o -d 'Set the binary communications program to use' -xa 'port-monitor=none port-monitor=bcp port-monitor=tbcp'
complete -c lpadmin -s o -d "Set error policy if printer backend can't send job" -xa 'printer-error-policy=abort-job printer-error-policy=retry-job printer-error-policy=retry-current-job printer-error-policy=stop-printer'
complete -c lpadmin -s o -xa printer-is-shared=true -d 'Sets dest to shared/published or unshared/unpublished'
complete -c lpadmin -s o -xa printer-is-shared=false -d 'Sets dest to shared/published or unshared/unpublished'
complete -c lpadmin -s o -d 'Set IPP operation policy associated with dest' -xa "printer-policy=(test -r /etc/cups/cupsd.conf; and string replace -r --filter '<Policy (.*)>' '$1' < /etc/cups/cupsd.conf)"

complete -c lpadmin -s u -xa 'allow:all allow:none (__fish_complete_list , __fish_complete_users allow:)' -d 'Sets user-level access control on a destination'
complete -c lpadmin -s u -xa '(__fish_complete_list , __fish_complete_groups allow: @)' -d 'Sets user-level access control on a destination'
complete -c lpadmin -s u -xa 'deny:all deny:none (__fish_complete_list , __fish_complete_users deny:)' -d 'Sets user-level access control on a destination'
complete -c lpadmin -s u -xa '(__fish_complete_list , __fish_complete_groups deny: @)' -d 'Sets user-level access control on a destination'
