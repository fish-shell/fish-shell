# Archlinux's mkinitcpio (https://projects.archlinux.org/mkinitcpio.git/)

function __fish_mkinitcpio_complete_hooks
	mkinitcpio --listhooks | tail -n +2 | sed -e "s/^[¹²³].*//" -e "s/\t\+/\n/g" | sed -e "s/^\([^[:space:]]\+\)[¹²³]\$/\1\t(deprecated)/g"
end
	
complete -c mkinitcpio -s A -l addhooks -d 'Add the additional hooks to the image' -a "(__fish_mkinitcpio_complete_hooks)" -f
complete -c mkinitcpio -s c -l config -d 'Use config file to generate the ramdisk'
complete -c mkinitcpio -s d -l generatedir -d 'Set directory as the location where the initramfs is built'
complete -c mkinitcpio -s g -l generate -d 'Generate a CPIO image as filename'
complete -c mkinitcpio -s H -l hookhelp -d 'Output help for a hook' -a "(__fish_mkinitcpio_complete_hooks)" -f
complete -c mkinitcpio -s h -l help -d 'Output a short overview of available command-line switches' -f
# Since we hardcode the path, we can also hardcode the number of slashes - saves us having to fork a sed
complete -c mkinitcpio -s k -l kernel -d 'Use kernelversion, instead of the current running kernel' -a "(printf "%s\n" /usr/lib/modules/* | grep -v "extramodules" | cut -d"/" -f5)"
complete -c mkinitcpio -s L -l listhooks -d 'List all available hooks' -f
complete -c mkinitcpio -s M -l automods -d 'Display modules found via autodetection' -f
complete -c mkinitcpio -s n -l nocolor -d 'Disable color output' -f
complete -c mkinitcpio -s P -l allpresets -d 'Process all presets contained in /etc/mkinitcpio.d' -f
complete -c mkinitcpio -s p -l preset -d 'Build initramfs image(s) according to specified preset'
complete -c mkinitcpio -s r -l moduleroot -d 'Specifies the root directory to find modules in'
complete -c mkinitcpio -s S -l skiphooks -d 'Skip hooks when generating the image' -a "(__fish_mkinitcpio_complete_hooks)" -f
complete -c mkinitcpio -s s -l save -d 'Saves the build directory for the initial ramdisk'
complete -c mkinitcpio -s t -l builddir -d 'Use tmpdir as the temporary build directory instead of /tmp'
complete -c mkinitcpio -s V -l version -d 'Display version information'
complete -c mkinitcpio -s v -l verbose -d 'Verbose output'
complete -c mkinitcpio -s z -l compress -d 'Override the compression method with the compress program'

