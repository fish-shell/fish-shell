#!/usr/bin/env python
# -*- python -*-

# Program to generate fish completion function for wajig.
# It runs 'wajig command' and analyzes the output to build a
# completion file which it writes to stdout.
# To use the result, direct stdout to
# ~/.fish.d/completions/wajig.fish.

# Author Reuben Thomas, from Don Rozenberg's bash_completion.py and
# fish's apt-get.fish.

import os
import re
import pprint
pp = pprint.PrettyPrinter()

def escape_quotes(s):
  return re.sub('\'', '\\\'', s)

# Run wajig command
f = os.popen('wajig commands', 'r')

lines = f.readlines()

option_patt = r'^-([a-z]*)\|--([a-z]*) +([^ ].*)'
option_patt_r = re.compile(option_patt)

command_patt = r'^([a-z-]*) +([^ ].*)'
command_patt_r = re.compile(command_patt)

os_str = []
os_str.append('')
ol_str = []
ol_str.append('')
oh_str = []
oh_str.append('')
o_i = 0

c_str = []
c_str.append('')
ch_str = []
ch_str.append('')
c_i = 0

for l in lines:
    l = l.strip() 
    if l == '' or l.find(':') > -1 or l.find('Run') == 0:
        continue
    if l.find('-') == 0:
        mo = option_patt_r.search(l)
        if mo == None:
            continue
        os_str[o_i] = mo.group(1)
        os_str.append('')
        ol_str[o_i] = mo.group(2)
        ol_str.append('')
        oh_str[o_i] = escape_quotes(mo.group(3))
        oh_str.append('')
        o_i += 1
    else:
        mo = command_patt_r.search(l)
        if mo == None:
            continue
        c_str[c_i] = mo.group(1)
        c_str.append('')
        ch_str[c_i] = escape_quotes(mo.group(2))
        ch_str.append('')
        c_i += 1

# For debugging, print the commands and options.
#print
#pp.pprint(c_str)

#print
#pp.pprint(os_str)
#print
#pp.pprint(ol_str)

part1 = '''function __fish_wajig_no_subcommand -d (N_ 'Test if wajig has yet to be given the subcommand')
	for i in (commandline -opc)
		if contains -- $i'''

part2 = '''
			return 1
		end
	end
	return 0
end

function __fish_wajig_use_package -d (N_ 'Test if wajig command should have packages as potential completion')
	for i in (commandline -opc)
		if contains -- $i contains bug build build-depend changelog dependents describe detail hold install installr installrs installs list list-files news package purge purge-depend readme recursive recommended reconfigure reinstall remove remove-depend repackage show showinstall showremove showupgrade size sizes source suggested unhold upgrade versions whatis
			return 0
		end
	end
	return 1
end

complete -c wajig -n '__fish_wajig_use_package' -a '(__fish_print_packages)' -d (N_ 'Package')'''

wajig = part1

#add the commands.
for i in range(0, len(c_str) - 1):
    wajig = "%s %s" % (wajig, c_str[i])

#add part2
wajig = "%s%s" % (wajig, part2)

#add the options.
wajig = "%s%s" % (wajig, os_str[0].lstrip())
for i in range(1, len(os_str) - 1):
    wajig = "%s\ncomplete -c apt-get -s %s -l %s -d (N_ '%s')" % (wajig, os_str[i], ol_str[i], oh_str[i])

#add the commands.
for i in range(0, len(c_str) - 1):
    wajig = "%s\ncomplete -f -n '__fish_wajig_no_subcommand' -c wajig -a '%s' -d(N_ '%s')" % (wajig, c_str[i], ch_str[i])

#print it all
print wajig
