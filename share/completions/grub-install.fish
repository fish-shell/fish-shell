# grub-install - install GRUB to a device

complete -c grub-install -l compress -d 'Compress GRUB files' -x -a 'no xz gz lzo'
complete -c grub-install -l directory -s d -d 'Use image and modules under given directory' -r
complete -c grub-install -l fonts -d 'Install given fonts' -r
complete -c grub-install -l install-modules -d 'Install only given modules and their dependencies' -x
complete -c grub-install -l pubkey -s k -d 'Embed given file as public key for signature checking' -r
complete -c grub-install -l locale-directory -d 'Use translations under given directory' -r
complete -c grub-install -l locales -d 'Install only given locales' -x
complete -c grub-install -l modules -d 'Preload specfied modules' -x
complete -c grub-install -l themes -d 'Install given theme' -x
complete -c grub-install -l verbose -s v -d 'Verbose mode'
complete -c grub-install -l allow-floppy -d 'Make drive also bootable as floppy'
complete -c grub-install -l boot-directory -d 'Install GRUB images under DIR/grub' -r
complete -c grub-install -l bootloader-id -d 'Set bootloader ID' -x
complete -c grub-install -l core-compress -d 'Choose compression to use for core image' -x -a 'none xz auto'
complete -c grub-install -l disk-module -d 'Set disk module to use' -x
complete -c grub-install -l efi-directory -d 'Use given directory as the EFI System Partition root' -r
complete -c grub-install -l force -d 'Install even if problems are detected'
complete -c grub-install -l force-file-id -d 'Use identifier file even if UUID is available'
complete -c grub-install -l label-bgcolor -d 'Use given color for label background' -x
complete -c grub-install -l label-color -d 'Use given color for label' -x
complete -c grub-install -l label-font -d 'Use given file for label' -r
complete -c grub-install -l macppc-directory -d 'Use given directory for PPC Mac install' -r
complete -c grub-install -l no-bootsector -d 'Do not install bootsector'
complete -c grub-install -l no-nvram -d 'Don\'t update the boot-device/Boot* NVRAM variables'
complete -c grub-install -l no-rs-codes -d 'Don\'t apply any reed-solomon codes when embedding core.img'
complete -c grub-install -l product-version -d 'Set product version' -x
complete -c grub-install -l recheck -d 'Delete device map if it already exists'
complete -c grub-install -l removable -d 'The installation is removable'
complete -c grub-install -l skip-fs-probe -s s -d 'Do not probe for filesystems'
complete -c grub-install -l target -d 'Install GRUB for given platform' -a 'arm-efi\t arm-uboot\t arm64-efi\t i386-coreboot\t i386-efi\t i386-ieee1275\t i386-multiboot\t i386-pc\t i386-qemu\t i386-xen\t ia64-efi\t mips-arc\t mips-qemu_mips\t mipsel-arc\t mipsel-loongson\t mipsel-qemu_mips\t powerpc-ieee1275\t sparc64-ieee1275\t x86_64-efi\t x86_64-xen\t' -x
complete -c grub-install -l help -s '?' -d 'Show help'
complete -c grub-install -l usage -d 'Show usage'
complete -c grub-install -l version -d 'Show version'
