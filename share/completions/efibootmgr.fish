function __efibootmgr_list_bootnum
    efibootmgr | string match -r -- "Boot\d+.+" |
        string split -m1 -f2 -- Boot |
        string replace -r -- "\* |  " "\t"
end

complete -f -c efibootmgr -s a -l active -d "sets bootnum active"
complete -f -c efibootmgr -s A -l inactive -d "sets bootnum inactive"
complete -f -c efibootmgr -s b -l bootnum -d "modify BootXXXX (hex)" -ra '(__efibootmgr_list_bootnum)'
complete -f -c efibootmgr -s B -l delete-bootnum -d "delete bootnum"
complete -f -c efibootmgr -s c -l create -d "create new variable bootnum and add to bootorder"
complete -f -c efibootmgr -s C -l create-only -d "create new variable bootnum and do not add to bootorder"
complete -f -c efibootmgr -s D -l remove-dups -d "remove duplicate values from BootOrder"
complete -F -c efibootmgr -s d -l disk -d "disk containing loader (Default: /dev/sda)"
complete -f -c efibootmgr -s r -l driver -d "Operate on Driver variables, not Boot Variables."
complete -f -c efibootmgr -s e -l edd -d "force EDD 1.0 or 3.0 creation variables, or guess" -ra "1 3 -1"
complete -f -c efibootmgr -s E -l device -d "EDD 1.0 device number (Default: 0x80)" -r
complete -f -c efibootmgr -s g -l gpt -d "force disk with invalid PMBR to be treated as GPT"
complete -F -c efibootmgr -s i -l iface -d "create a netboot entry for the named interface" -r
complete -F -c efibootmgr -s l -l loader -d "EFI loader file location (Default: \EFI\BOOT\grub.efi)" -r
complete -f -c efibootmgr -s L -l label -d "Boot manager display label (Default: Linux)"
complete -f -c efibootmgr -s m -l mirror-below-4G -d "t|f mirror memory below 4GB"
complete -f -c efibootmgr -s M -l mirror-above-4G -d "X percentage memory to mirror above 4GB"
complete -f -c efibootmgr -s n -l bootnext -d "set BootNext to XXXX (hex)" -ra '(__efibootmgr_list_bootnum)'
complete -f -c efibootmgr -s N -l delete-bootnext -d "delete BootNext"
complete -f -c efibootmgr -s o -l bootorder -d "explicitly set BootOrder XXXX,YYYY,ZZZZ (hex)"
complete -f -c efibootmgr -s O -l delete-bootorder -d "delete BootOrder"
complete -f -c efibootmgr -s p -l part -d "partition containing loader (Default: 1)"
complete -f -c efibootmgr -s q -l quiet -d "be quiet"
complete -f -c efibootmgr -s t -l timeout -d "set boot manager timeout waiting for user input." -r
complete -f -c efibootmgr -s T -l delete-timeout -d "delete Timeout."
complete -f -c efibootmgr -s u -l unicode -l UCS-2 -d "handle extra args as Unicode(UCS-2)"
complete -f -c efibootmgr -s v -l verbose -d "print additional information"
complete -f -c efibootmgr -s V -l version -d "return version and exit"
complete -f -c efibootmgr -s w -l write-signature -d "write unique sig to MBR if needed"
complete -f -c efibootmgr -s y -l sysprep -d "Operate on SysPrep variables, not Boot Variables."
complete -F -c efibootmgr -s @ -l append-binary-args -d "append extra args from file (use - for stdin)" -r
complete -f -c efibootmgr -s h -l help -d "show help/usage"
