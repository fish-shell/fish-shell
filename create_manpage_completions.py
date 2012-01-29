#!/usr/bin/python

""" Run me like this: ./ManParser2.py /usr/share/man/man1/* > test2.out """

import sys, re, os.path, gzip

# This gets set to the name of the command that we are currently executing
CMDNAME = ""


def compileAndSearch(regex, input):
	options_section_regex = re.compile(regex , re.DOTALL)
	options_section_matched = re.search( options_section_regex, input)
	return options_section_matched

def unquoteDoubleQuotes(data):
	if (len(data) < 2):
		return data
	if data[0] == '"' and data[len(data)-1] == '"':
		data = data[1:len(data)-1]
	return data

def unquoteSingleQuotes(data):
	if (len(data) < 2):
		return data
	if data[0] == '`' and data[len(data)-1] == '\'':
		data = data[1:len(data)-1]
	return data

def printcompletecommand(cmdname, args, description):
	print "complete -c", cmdname,
	for arg in args:
		print arg,
#	print "--descripton ", description
	print "\n",

def builtcommand(options, description):
#	print "Options are: ", options
	optionlist = re.split(" |,|\"|=|[|]", options)
	optionlist = filter(lambda x: len(x) > 0 and x[0] == "-", optionlist)
	if len(optionlist) == 0:
		return
	for i in range(0, len(optionlist)):
		if optionlist[i][0:2] == "--":
			optionlist[i] = "-l " + optionlist[i][2:]
		else:
			optionlist[i] = "-s " + optionlist[i][1:]

	printcompletecommand(CMDNAME, optionlist, description)

	

def removeGroffFormatting(data):
#	data = data.replace("\fI","")
#	data = data.replace("\fP","")
	data = data.replace("\\fI","")
	data = data.replace("\\fP","")
	data = data.replace("\\f1","")
	data = data.replace("\\fB","")
	data = data.replace("\\fR","")
	data = data.replace("\\e","")
	data = re.sub(".PD( \d+)","",data)
	data = data.replace(".BI","")
	data = data.replace(".BR","")
	data = data.replace("0.5i","")
	data = data.replace(".rb","")
	data = data.replace("\\^","")
	data = data.replace("{ ","")
	data = data.replace(" }","")
	data = data.replace("\ ","")
	data = data.replace("\-","-")
	data = data.replace("\&","")
	data = data.replace(".B","")
	data = data.replace("\-","-")
	data = data.replace(".I","")
	data = data.replace("\f","")
	return data

class ManParser:
	def isMyType(self, manpage):
		return False

	def parseManPage(self, manpage):
		return False

	def name(self):
		return "no-name"

class Type1ManParser(ManParser):
	def isMyType(self, manpage):
		#   print manpage
		options_section_matched = compileAndSearch("\.SH \"OPTIONS\"(.*?)", manpage)

		if options_section_matched == None:
			return False
		else:
			return True

	def parseManPage(self, manpage):
		options_section_regex = re.compile( "\.SH \"OPTIONS\"(.*?)(\.SH|\Z)", re.DOTALL)
		options_section_matched = re.search( options_section_regex, manpage)
		
		options_section = options_section_matched.group(0)
		#   print options_section
		options_parts_regex = re.compile("\.PP(.*?)\.RE", re.DOTALL)
		options_matched = re.search(options_parts_regex, options_section)
		#   print options_matched
		print >> sys.stderr, "Command is ", CMDNAME

		if options_matched == None:
			print >> sys.stderr, "Unable to find options"
			if( self.fallback(options_section) ):
				return True
			elif (self.fallback2(options_section) ):
				return True
			return False
		
		while (options_matched != None):
			#       print len(options_matched.groups())
			#       print options_matched.group()
			data = options_matched.group(1)
			last_dotpp_index = data.rfind(".PP")
			if (last_dotpp_index != -1):
				data = data[last_dotpp_index+3:]

			data = removeGroffFormatting(data)
			data = data.split(".RS 4")
			#       print data
			if (len (data) > 1): #and len(data[1]) <= 300):
				optionName = data[0].strip()
				
				if ( optionName.find("-") == -1):
					print >> sys.stderr, optionName, " doesn't contain - "	
#					return False
				else:
					optionName = unquoteDoubleQuotes(optionName)
					optionName = unquoteSingleQuotes(optionName)
					optionDescription = data[1].strip().replace("\n"," ")
#					print >> sys.stderr, "Option: ", optionName," Description: ", optionDescription , '\n'
					builtcommand(optionName, optionDescription)
					
			else:
				print >> sys.stderr, "Unable to split option from description"
				return False

			options_section = options_section[options_matched.end()-3:]
			options_matched = re.search(options_parts_regex, options_section)

	def fallback(self, options_section):
		print >> sys.stderr, "Falling Back"
		options_parts_regex = re.compile("\.TP( \d+)?(.*?)\.TP", re.DOTALL)
		options_matched = re.search(options_parts_regex, options_section)
		if options_matched == None:
			print >> sys.stderr, "Still not found"
			return False
		while options_matched != None:
			data = options_matched.group(2)
			data = removeGroffFormatting(data)
			data = data.strip()
			data = data.split("\n",1)
			if (len(data)>1 and len(data[1].strip())>0): # and len(data[1])<400):
				optionName = data[0].strip()
				if ( optionName.find("-") == -1):
					print >> sys.stderr, optionName, "doesn't contains -"
				else:
					optionName = unquoteDoubleQuotes(optionName)
					optionName = unquoteSingleQuotes(optionName)
					optionDescription = data[1].strip().replace("\n"," ")
#					print "Option: ", optionName," Description: ", optionDescription , '\n'
					builtcommand(optionName, optionDescription)
			else:
				print >> sys.stderr, data
				print >> sys.stderr, "Unable to split option from description"
				return False

			options_section = options_section[options_matched.end()-3:]
			options_matched = re.search(options_parts_regex, options_section)
		return True

	def fallback2(self, options_section):
		print >> sys.stderr, "Falling Back2"
		ix_remover_regex = re.compile("\.IX.*")
		trailing_num_regex = re.compile('\\d+$')
		options_parts_regex = re.compile("\.IP (.*?)\.IP", re.DOTALL)

		options_section = re.sub(ix_remover_regex, "", options_section)
		options_matched = re.search(options_parts_regex, options_section)
		if options_matched == None:
			print >> sys.stderr, "Still not found2"
			return False
		while options_matched != None:
			data = options_matched.group(1)
			
#			print "Data is : ", data
			data = removeGroffFormatting(data)
			data = data.strip()
			data = data.split("\n",1)
			if (len(data)>1 and len(data[1].strip())>0): # and len(data[1])<400):
#				print "Data[0] is: ", data[0]

#				data = re.sub(trailing_num_regex, "", data)
				optionName = re.sub(trailing_num_regex, "", data[0].strip())
				
				if ( optionName.find("-") == -1):
					print >> sys.stderr, optionName, "doesn't contains -"
				else:
					optionName = optionName.strip()
					optionName = unquoteDoubleQuotes(optionName)
					optionName = unquoteSingleQuotes(optionName)
					optionDescription = data[1].strip().replace("\n"," ")
#					print "Option: ", optionName," Description: ", optionDescription , '\n'
					builtcommand(optionName, optionDescription)
			else:
#				print data
				print >> sys.stderr, "Unable to split option from description"
				return False

			options_section = options_section[options_matched.end()-3:]
			options_matched = re.search(options_parts_regex, options_section)
		return True
		
	def name(self):
		return "Type1"


class Type2ManParser(ManParser):
	def isMyType(self, manpage):
		options_section_matched = compileAndSearch("\.SH OPTIONS(.*?)", manpage)

		if options_section_matched == None:
			return False
		else:
			return True

	def parseManPage(self, manpage):
		options_section_regex = re.compile( "\.SH OPTIONS(.*?)(\.SH|\Z)", re.DOTALL)
		options_section_matched = re.search( options_section_regex, manpage)
		
#		if (options_section_matched == None):
#			print "Falling Back"
#			options_section_regex = re.compile( "\.SH OPTIONS(.*?)$", re.DOTALL)
#			options_section_matched = re.search( options_section_regex, manpage)
#		print manpage
		options_section = options_section_matched.group(1)
#		print options_section
		#   print options_section
		#   sys.exit(1)

#		options_parts_regex = re.compile("\.TP(.*?)\.TP", re.DOTALL)
		options_parts_regex = re.compile("\.[I|T]P( \d+(\.\d)?i?)?(.*?)\.[I|T]P", re.DOTALL)
#		options_parts_regex = re.compile("\.TP(.*?)[(\.TP)|(\.SH)]", re.DOTALL)
		options_matched = re.search(options_parts_regex, options_section)
		print >> sys.stderr, "Command is ", CMDNAME

		if options_matched == None:
			print >> sys.stderr, self.name() + ": Unable to find options"
			return False

		while (options_matched != None):
			#       print len(options_matched.groups())
			data = options_matched.group(3)

			data = removeGroffFormatting(data)

			data = data.strip()

			data = data.split("\n",1)
#			print >> sys.stderr, data
			if (len(data)>1 and len(data[1].strip())>0): # and len(data[1])<400):
				optionName = data[0].strip()
				if ( optionName.find("-") == -1):
					print >> sys.stderr, optionName, "doesn't contains -"
				else:
					optionName = unquoteDoubleQuotes(optionName)
					optionName = unquoteSingleQuotes(optionName)
					optionDescription = data[1].strip().replace("\n"," ")
#					print "Option: ", optionName," Description: ", optionDescription , '\n'
					builtcommand(optionName, optionDescription)
			else:
				print >> sys.stderr, data
				print >> sys.stderr, "Unable to split option from description"
				
#				return False

			options_section = options_section[options_matched.end()-3:]
			options_matched = re.search(options_parts_regex, options_section)

	

	def name(self):
		return "Type2"


class Type3ManParser(ManParser):
	def isMyType(self, manpage):
		options_section_matched = compileAndSearch("\.SH DESCRIPTION(.*?)", manpage)

		if options_section_matched == None:
			return False
		else:
			return True

	def parseManPage(self, manpage):
		options_section_regex = re.compile( "\.SH DESCRIPTION(.*?)(\.SH|\Z)", re.DOTALL)
		options_section_matched = re.search( options_section_regex, manpage)
		
		options_section = options_section_matched.group(1)
		#   print options_section
		#   sys.exit(1)
		options_parts_regex = re.compile("\.TP(.*?)\.TP", re.DOTALL)
		options_matched = re.search(options_parts_regex, options_section)
		print >> sys.stderr, "Command is ", CMDNAME

		if options_matched == None:
			print >> sys.stderr, "Unable to find options section"
			return False

		while (options_matched != None):
#			print len(options_matched.groups())
			data = options_matched.group(1)

			data = removeGroffFormatting(data)
			data = data.strip()
			data = data.split("\n",1)

			if (len(data)>1): # and len(data[1])<400):
				optionName = data[0].strip()
				if ( optionName.find("-") == -1):
					print >> sys.stderr, optionName, "doesn't contains -"
				else:
					optionName = unquoteDoubleQuotes(optionName)
					optionName = unquoteSingleQuotes(optionName)
					optionDescription = data[1].strip().replace("\n"," ")
#					print >> sys.stderr, "Option: ", optionName," Description: ", optionDescription , '\n'
					builtcommand(optionName, optionDescription)

			else:
				print >> sys.stderr, "Unable to split option from description"
				return False

			options_section = options_section[options_matched.end()-3:]
			options_matched = re.search(options_parts_regex, options_section)



	def name(self):
		return "Type3"


class Type4ManParser(ManParser):
	def isMyType(self, manpage):
		options_section_matched = compileAndSearch("\.SH FUNCTION LETTERS(.*?)", manpage)

		if options_section_matched == None:
			return False
		else:
			return True

	def parseManPage(self, manpage):
		options_section_regex = re.compile( "\.SH FUNCTION LETTERS(.*?)(\.SH|\Z)", re.DOTALL)
		options_section_matched = re.search( options_section_regex, manpage)
		
		options_section = options_section_matched.group(1)
		#   print options_section
		#   sys.exit(1)
		options_parts_regex = re.compile("\.TP(.*?)\.TP", re.DOTALL)
		options_matched = re.search(options_parts_regex, options_section)
		print >> sys.stderr, "Command is ", CMDNAME

		if options_matched == None:
			print >> sys.stderr, "Unable to find options section"
			return False

		while (options_matched != None):
			#       print len(options_matched.groups())
			data = options_matched.group(1)

			data = removeGroffFormatting(data)
			data = data.strip()
			data = data.split("\n",1)

			if (len(data)>1): # and len(data[1])<400):
				optionName = data[0].strip()
				if ( optionName.find("-") == -1):
					print >> sys.stderr, optionName, "doesn't contains -"
				else:
					optionName = unquoteDoubleQuotes(optionName)
					optionName = unquoteSingleQuotes(optionName)
					optionDescription = data[1].strip().replace("\n"," ")
#					print "Option: ", optionName," Description: ", optionDescription , '\n'
					builtcommand(optionName, optionDescription)

			else:
				print >> sys.stderr, "Unable to split option from description"
				return False

			options_section = options_section[options_matched.end()-3:]
			options_matched = re.search(options_parts_regex, options_section)

		return True

	def name():
		return "Type4"


def parse_manpage_at_path(manpage_path):
	filename = os.path.basename(manpage_path)

	# Get the "base" command, e.g. gcc.1.gz -> gcc
	global CMDNAME
	CMDNAME = filename.split('.', 1)[0]

	print >> sys.stderr, "Considering " + manpage_path
	if manpage_path.endswith('.gz'):
		fd = gzip.open(manpage_path, 'r')
	else:
		fd = open(manpage_path, 'r')
	manpage = fd.read()
	fd.close()
	
	parsers = [Type1ManParser(), Type2ManParser(), Type4ManParser(), Type3ManParser()]
	parserToUse = None

	# Get the "base" command, e.g. gcc.1.gz -> gcc
	cmd_base = CMDNAME.split('.', 1)[0]
	ignoredcommands = ["cc", "g++", "gcc", "c++", "cpp", "emacs", "gprof", "wget", "ld", "awk"]
	if cmd_base in ignoredcommands:
		return
		
	for parser in parsers:
		if parser.isMyType(manpage):
			parserToUse = parser
#			print "Type is: ", parser.name()
			break
		
	if parserToUse == None:
		print >> sys.stderr, manpage_path, " : Not supported"

	elif parserToUse == parsers[0]:
		if parserToUse.parseManPage(manpage) == False:
			print >> sys.stderr, "Type1 : ", manpage_path, " is unparsable"
		else:
			print >> sys.stderr, manpage_path, " parsed successfully"
	
	elif parserToUse == parsers[1]:
		if parserToUse.parseManPage(manpage) == False:
			print >> sys.stderr, "Type2 : ", manpage_path, " is unparsable"
		else:
			print >> sys.stderr, "Type2 : ", manpage_path, " parsed successfully"
	elif parserToUse == parsers[2]:
		if parserToUse.parseManPage(manpage) == False:
			print >> sys.stderr, "Type3 : ", manpage_path, " is unparsable"
		else:
			print >> sys.stderr, manpage_path, " parsed successfully"

	elif parserToUse == parsers[3]:
		if parserToUse.parseManPage(manpage) == False:
			print >> sys.stderr, "Type4 : ", manpage_path, " is unparsable"
		else:
			print >> sys.stderr, "Type4 :", manpage_path, " parsed successfully"

		

def compare_paths(a, b):
	""" Compare two paths by their base name, case insensitive """
	return cmp(os.path.basename(a).lower(), os.path.basename(b).lower())

if __name__ == "__main__":
	paths = sys.argv[1:]
	paths.sort(compare_paths)
	for manpage_path in paths:
		try:
			parse_manpage_at_path(manpage_path)
		except IOError:
			print >> sys.stderr, 'Cannot open ', manpage_path
		except:
			print >> sys.stderr, "Error parsing %s: %s" % (manpage_path, sys.exc_info()[0])
