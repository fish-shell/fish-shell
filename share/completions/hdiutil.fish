# hdiutil - manipulate disk images: attach, verify, create, etc (man 1 hdiutil)
#
# hdiutil uses a subcommand grammar: `hdiutil <verb> [options]`. The first
# non-option token is the verb. Most verbs take an image file or a /dev device
# as their operand. detach/eject/mountvol take an attached device, which we
# enumerate live from `hdiutil info`.

# ── command-line introspection ───────────────────────────────────────
# Print the chosen verb (first non-dash token after the command), if any.
function __fish_hdiutil_verb
    set -l toks (commandline -opc)
    set -e toks[1]
    for t in $toks
        if not string match -q -- '-*' $t
            echo $t
            return 0
        end
    end
    return 1
end

# True when no verb has been chosen yet (so verbs should be offered).
function __fish_hdiutil_no_verb
    not __fish_hdiutil_verb >/dev/null 2>&1
end

# ── live enumerators ─────────────────────────────────────────────────
# Attached /dev disk nodes, parsed from `hdiutil info` (fast, unprivileged).
function __fish_hdiutil_devices
    hdiutil info 2>/dev/null | string match -r -- '/dev/disk[0-9]+s?[0-9]*' \
        | string match -r -- '^/dev/disk[0-9s]+$' | sort -u
end

# ── verbs ────────────────────────────────────────────────────────────
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a help -d 'Display minimal usage information for each verb'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a attach -d 'Attach a disk image as a device'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a mount -d 'Synonym for attach (poorly-named)'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a detach -d 'Detach a disk image and terminate associated process'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a eject -d 'Synonym for detach'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a verify -d 'Compute and verify the checksum of a read-only or compressed image'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a create -d 'Create a new image of the given size or from provided data'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a convert -d 'Convert an image to another format'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a compact -d 'Reclaim unused bands in a sparse (UDSP/UDSB) image'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a resize -d 'Resize an image and/or the filesystem within it'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a burn -d 'Burn an image to optical media'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a checksum -d 'Calculate a checksum on the image data'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a chpass -d 'Change the passphrase for an encrypted image'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a erasekeys -d 'Erase the cryptographic keys of an encrypted image'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a fsid -d 'Print information about filesystems on a disk image'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a imageinfo -d 'Display detailed information about a disk image'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a isencrypted -d 'Print whether an image is encrypted'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a info -d 'Display info about the disk image driver and attached images'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a mountvol -d 'Mount the volume on an already-attached device'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a unmount -d 'Unmount a volume without detaching the image'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a plugins -d 'Display information about installed DiskImages plugins'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a pmap -d 'Display the partition map of an image'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a segment -d 'Segment an image into multiple files'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a makehybrid -d 'Generate a cross-platform (HFS+/ISO/Joliet/UDF) image'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a udifrez -d 'Store a resource in a UDIF image (deprecated)'
complete -c hdiutil -f -n __fish_hdiutil_no_verb -a udifderez -d 'Extract a resource from a UDIF image (deprecated)'

# ── common options (understood by many verbs) ────────────────────────
complete -c hdiutil -n 'not __fish_hdiutil_no_verb' -l verbose -d 'Produce extra progress output and error diagnostics'
complete -c hdiutil -n 'not __fish_hdiutil_no_verb' -l quiet -d 'Close stdout/stderr; only exit status indicates success'
complete -c hdiutil -n 'not __fish_hdiutil_no_verb' -l debug -d 'Be very verbose (implies -verbose)'
complete -c hdiutil -n 'not __fish_hdiutil_no_verb' -l help -d 'Print basic usage information for this verb'

set -l img_verbs verify create convert compact resize burn checksum chpass erasekeys \
    fsid imageinfo isencrypted pmap segment udifrez udifderez attach mount makehybrid
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $img_verbs" -l plist -d 'Provide result output in plist format'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $img_verbs" -l puppetstrings -d 'Provide progress output easy for another program to parse'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $img_verbs" -l srcimagekey -x -d 'key=value for the image recognition system'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $img_verbs" -l imagekey -x -d 'Synonym for -srcimagekey'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $img_verbs" -l encryption -x -a 'AES-128 AES-256' -d 'Specify the encryption algorithm'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $img_verbs" -l stdinpass -d 'Read a null-terminated passphrase from standard input'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $img_verbs" -l shadow -r -d 'Use a shadow file alongside the primary image'

# ── attach / mount ───────────────────────────────────────────────────
# Operand: a disk image file.
set -l attach_verbs attach mount
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" \
    -k -a '(__fish_complete_suffix .dmg .iso .cdr .sparseimage .sparsebundle .img)'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l readonly -d 'Force the resulting device to be read-only'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l readwrite -d 'Override the framework decision and attach read/write'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l nomount -d 'Identical to -mount suppressed'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l mount -x -a 'required optional suppressed' -d 'Whether filesystems in the image should be mounted'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l mountpoint -r -d 'Mount the single volume at this path instead of /Volumes'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l mountroot -r -d 'Mount volumes under this directory instead of /Volumes'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l mountrandom -r -d 'Like -mountroot, with randomized mount-point names'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l nobrowse -d 'Render volumes invisible in the Finder'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l owners -x -a 'on off' -d 'Honor owners on the filesystems or not'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l noverify -d 'Do not verify the image before attaching'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l verify -d 'Verify the image before attaching'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l nokernel -d 'Attach with a helper process (the default)'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l kernel -d 'Attempt to attach in-kernel; fail if unsupported'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $attach_verbs" -l section -x -d 'Attach only a subsection (offset / first-last / start,count)'

# ── detach / eject ───────────────────────────────────────────────────
# Operand: an attached /dev device (live).
set -l detach_verbs detach eject
complete -c hdiutil -x -n "contains -- (__fish_hdiutil_verb) $detach_verbs" -a '(__fish_hdiutil_devices)' -d 'Attached device'
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $detach_verbs" -l force -d 'Ignore open files on mounted volumes'

# ── mountvol ─────────────────────────────────────────────────────────
complete -c hdiutil -x -n '__fish_hdiutil_verb | string match -q mountvol' -a '(__fish_hdiutil_devices)' -d 'Attached device'

# ── unmount ──────────────────────────────────────────────────────────
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q unmount' -l force -d 'Unmount regardless of open files on the filesystem'

# ── verbs taking an image-file operand ───────────────────────────────
set -l file_op_verbs verify convert compact resize burn checksum chpass erasekeys \
    fsid imageinfo isencrypted pmap segment udifrez udifderez
complete -c hdiutil -n "contains -- (__fish_hdiutil_verb) $file_op_verbs" \
    -k -a '(__fish_complete_suffix .dmg .iso .cdr .sparseimage .sparsebundle .img)'

# ── create ───────────────────────────────────────────────────────────
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q create' -l size -x -d 'Image size (e.g. 100m, 4g) in mkfile(8) style'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q create' -l sectors -x -d 'Image size in 512-byte sectors'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q create' -l megabytes -x -d 'Image size in megabytes'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q create' -l fs -x -a 'HFS+ HFS+J JHFS+ HFSX APFS ExFAT MS-DOS UDF' -d 'Filesystem to make on the new image'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q create' -l volname -x -d 'Volume name for the new filesystem'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q create' -l type -x -a 'UDIF SPARSE SPARSEBUNDLE' -d 'Format of the empty read/write image'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q create' -l layout -x -d 'Partition layout (e.g. SPUD, GPTSPUD, NONE)'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q create' -l srcfolder -r -d 'Copy the contents of this folder into the image'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q create' -l srcdevice -r -d 'Use the blocks of this device to create the image'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q create' -l align -x -d 'Alignment of the final data partition'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q create' -l ov -d 'Overwrite an existing image at the destination'

# ── convert ──────────────────────────────────────────────────────────
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q convert' -l format -x \
    -a 'UDRW UDRO UDCO UDZO ULFO ULMO UDBZ UDTO UDSP UDSB UFBI' -d 'Target image format'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q convert' -o o -r -d 'Output file'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q convert' -l align -x -d 'Alignment for the output image'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q convert' -l pmap -d 'Add a partition map'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q convert' -l segmentSize -x -d 'Segment the output into size_spec-sized files (deprecated)'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q convert' -l tasks -x -d 'Number of threads to use for compression'

# ── compact ──────────────────────────────────────────────────────────
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q compact' -l batteryallowed -d 'Allow compacting while on battery power'

# ── resize ───────────────────────────────────────────────────────────
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q resize' -l size -x -d 'New size (e.g. 100m, 4g) in mkfile(8) style'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q resize' -l sectors -x -d 'New size in 512-byte sectors (or min)'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q resize' -l limits -d 'Display minimum, current, and maximum sizes; do not resize'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q resize' -l imageonly -d 'Resize only the image file, not the partition'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q resize' -l partitiononly -d 'Resize only a partition/filesystem in the image'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q resize' -l partitionID -x -d 'Partition to resize'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q resize' -l nofinalgap -d 'Allow eliminating the trailing gap'

# ── checksum ─────────────────────────────────────────────────────────
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q checksum' -l type -x -a 'UDIF-CRC32 UDIF-MD5 CRC32 MD5 SHA SHA1 SHA256 SHA384 SHA512' -d 'Checksum type to calculate'

# ── burn ─────────────────────────────────────────────────────────────
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q burn' -l device -x -d 'Device to use for burning'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q burn' -l testburn -d "Don't turn on the laser (test burn)"
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q burn' -l anydevice -d 'Allow burning to devices not qualified by Apple'

# ── segment ──────────────────────────────────────────────────────────
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q segment' -o o -r -d 'First segment output name'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q segment' -l segmentCount -x -d 'Number of segments'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q segment' -l segmentSize -x -d 'Segment size in sectors or mkfile(8) style'

# ── makehybrid ───────────────────────────────────────────────────────
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q makehybrid' -o o -r -d 'Output image file'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q makehybrid' -l hfs -d 'Generate an HFS+ filesystem'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q makehybrid' -l iso -d 'Generate an ISO9660 Level 2 filesystem'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q makehybrid' -l joliet -d 'Generate Joliet extensions to ISO9660'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q makehybrid' -l udf -d 'Generate a UDF filesystem'
complete -c hdiutil -n '__fish_hdiutil_verb | string match -q makehybrid' -l default-volume-name -x -d 'Default volume name for all filesystems'
