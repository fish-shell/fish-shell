#apt-setup

# magic completion safety check (do not remove this comment)
if not type -q apt-setup
    exit
end
complete -c apt-setup -a probe -d "Probe a CD"
complete -c apt-setup -s N -d "Run in non-interactive mode"

