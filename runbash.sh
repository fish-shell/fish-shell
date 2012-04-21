#!/bin/bash -l

bash_config=~/.config/fish/bash_config.fish
if [ -e $bash_config ]
then
	mv $bash_config $bash_config.backup
fi
touch $bash_config
echo "function set_default" >> $bash_config
echo "	if not set -q \$argv[1]" >> $bash_config
echo "		set -gx \$argv" >> $bash_config
echo "	end" >> $bash_config
echo "end" >> $bash_config
echo "PS1=$PS1" | python share/tools/import_bash_settings.py
alias | python share/tools/import_bash_settings.py
env | python share/tools/import_bash_settings.py

