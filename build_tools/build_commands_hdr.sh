#!/bin/sh

# Builds the commands.hdr file.
# Usage: build_commands_hdr.sh ${HELP_SRC} < commands_hdr.in > commands.hdr

rm -f command_list.tmp command_list_toc.tmp
for i in `printf "%s\n" $@ | LC_ALL=C.UTF-8 sort`; do
	echo "<hr>" >>command_list.tmp;
	cat $i >>command_list.tmp;
	echo >>command_list.tmp;
	echo >>command_list.tmp;
	NAME=`basename $i .txt`;
	echo '- <a href="#'$NAME'">'$NAME'</a>' >> command_list_toc.tmp;
	echo "Back to <a href='index.html#toc-commands'>command index</a>". >>command_list.tmp;
done
mv command_list.tmp command_list.txt
mv command_list_toc.tmp command_list_toc.txt
/usr/bin/env awk '{if ($0 ~ /@command_list_toc@/) { system("cat command_list_toc.txt"); }
                   else if ($0 ~ /@command_list@/){ system("cat command_list.txt");}
                   else{ print $0;}}'
