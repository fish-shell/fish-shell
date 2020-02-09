# Wireshark - Interactively dump and analyze network traffic

__fish_complete_wireshark wireshark

complete -c wireshark -l display -d 'Specifies the X display to use' -x
complete -c wireshark -l fullscreen -d 'Start Wireshark in full screen' -x
complete -c wireshark -s g -d 'After reading in a capture file using th e-r flag, go to the given packet number' -x
complete -c wireshark -s H -d 'Hide the capture info dialog during live packet capture'
complete -c wireshark -s j -d 'When no exact match is found by a -J filter, select the first package before'
complete -c wireshark -s J -d 'Jump to packet matching filter (display filter syntax)' -x
complete -c wireshark -s k -d 'Start the capture session immediately'
complete -c wireshark -s l -d 'Turn on automatic scrolling'
complete -c wireshark -s m -d 'Set the font name used for most text' -x
complete -c wireshark -s P -d 'Override a configuration or data path' -x # TODO
complete -c wireshark -s S -d 'Automatically update the packet display as packets are coming in'
