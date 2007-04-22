#!/usr/bin/env python

import sys
import commands
import re

def escape_quotes(s):
  return re.sub('\'', '\\\'', s)

def escape(s):
  return re.sub('([\'"#%*?])', r"\\\1", s)

def print_completion( cmd, switch_name, arg, desc ):

	offset=1
	switch_type = "o"

	if len(switch_name) == 2:
		switch_type = "s"

	if switch_name[1] == "-":
		switch_type = "l"
		offset=2

	arg_str = ""
	if arg == None:
		pass
	elif 	arg == "standard":
		arg_str = "-a 'c89 c99 gnu89 gnu99 c++98 gnu++98'"
	else:
		pass

	print "complete -c %s -%s %s %s --description '%s'" % (cmd, switch_type, escape( switch_name[offset:] ), arg_str, escape_quotes(desc))

def clean_whitespace( str ):
	clean_whitespace_prog0 = re.compile( r"-[ \t]*\n[ \t\r]+" )
	clean_whitespace_prog1 = re.compile( r"[ \n\t]+" )
	clean_whitespace_prog2 = re.compile( r"^[ \t\r]*", re.MULTILINE )
	str = clean_whitespace_prog0.sub( "", str )
	str = clean_whitespace_prog1.sub( " ", str )
	str = clean_whitespace_prog2.sub( "", str )
	return str



cmd = sys.argv[1]
man = commands.getoutput( "man %s | col -b" % cmd )
remainder = man

re1 = r"\n( *-[^ ,]*  *(|\n))+[^.]+"
prog1 = re.compile(re1, re.MULTILINE)

re2 = r"^(|=[^ ]*)( |\n)*(?P<switch>-[^ =./\n]+)(  *[^-\n ]*\n|)"
prog2 = re.compile(re2, re.MULTILINE)

re3 = r"^=(?P<arg>[^ ]*)"
prog3 = re.compile(re2, 0)

while True:

	match = prog1.search( remainder )  

	if match == None:
		break

#	print "yay match!!!\n"
	str = match.string[match.start():match.end()]

#	print str

	rem2 = str

	switch = []

	while True:
		match2 = prog2.search( rem2 )

		if match2 == None:
			break

		sw = match2.expand( r"\g<switch>" )
#		print "yay switch %s!!!\n" %sw

		switch.append( sw )

		rem2 = rem2[match2.end():]

	match_arg = prog3.search( rem2 )
	arg = None
	
	if match_arg != None:
		arg = match_arg.expand( r"\g<arg>" )
		rem2 = rem2[match_arg.end():]

	desc = clean_whitespace(rem2)

	if len( desc) > 8 and arg != None:

#		print "Yay desc '%s'!!\n" % desc

		for i in switch:
			print_completion( cmd, i, arg, desc )


	remainder = remainder[match.end():]

