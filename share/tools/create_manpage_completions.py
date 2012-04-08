#!/usr/bin/python
# -*- coding: utf-8 -*-

# Run me like this: ./create_manpage_completions.py /usr/share/man/man1/* > man_completions.fish

"""
<OWNER> = Siteshwar Vashisht 
<YEAR> = 2012 

Copyright (c) 2012, Siteshwar Vashisht 
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

import string, sys, re, os.path, gzip, traceback
from deroff import Deroffer

# This gets set to the name of the command that we are currently executing
CMDNAME = ""

# builtcommand writes into this global variable, yuck
built_command_output = []

# Diagnostic output
diagnostic_output = []
diagnostic_indent = 0

global VERBOSE
VERBOSE = False

def add_diagnostic(dgn, always = False):
    # Add a diagnostic message if VERBOSE is True or always is True
    if VERBOSE or always:
        diagnostic_output.append('   '*diagnostic_indent + dgn)
    
def flush_diagnostics(where):
    if diagnostic_output:
        output_str = '\n'.join(diagnostic_output) + '\n'
        where.write(output_str)
        diagnostic_output[:] = []

# Make sure we don't output the same completion multiple times, which can happen
# For example, xsubpp.1.gz and xsubpp5.10.1.gz
# This maps commands to lists of completions
already_output_completions = {}

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
    

# Make a string of characters that are deemed safe in fish without needing to be escaped
# Note that space is not included
g_fish_safe_chars = frozenset(string.ascii_letters + string.digits + '_+-|/:=<>@~')

def fish_escape_single_quote(str):
    # Escape a string if necessary so that it can be put in single quotes
    # If it has no non-safe chars, there's nothing to do
    if g_fish_safe_chars.issuperset(str):
        return str
    
    str = str.replace('\\', '\\\\') # Replace one backslash with two
    str = str.replace("'", "\\'") # Replace one single quote with a backslash-single-quote
    return "'" + str + "'"

def output_complete_command(cmdname, args, description, output_list):
    comps = ['complete -c', cmdname]
    comps.extend(args)
    if description:
        comps.append('--description')
        comps.append(description)
    output_list.append(' '.join(comps))

def builtcommand(options, description):
#    print "Options are: ", options
    man_optionlist = re.split(" |,|\"|=|[|]", options)
    fish_options = []
    for option in man_optionlist:
        if option.startswith('--'):
            # New style long option (--recursive)
            fish_options.append('-l ' + fish_escape_single_quote(option[2:]))
        elif option.startswith('-') and len(option) == 2:
            # New style short option (-r)
            fish_options.append('-s ' + fish_escape_single_quote(option[1:]))
        elif option.startswith('-') and len(option) > 2:
            # Old style long option (-recursive)
            fish_options.append('-o ' + fish_escape_single_quote(option[1:]))
    
    # Determine which options are new (not already in existing_options)
    # Then add those to the existing options
    existing_options = already_output_completions.setdefault(CMDNAME, set())
    fish_options = [opt for opt in fish_options if opt not in existing_options]
    existing_options.update(fish_options)
    
    # Maybe it's all for naught
    if not fish_options: return
    
    first_period = description.find(".") 
    if first_period >= 45 or (first_period == -1 and len(description) > 45):
        description = description[:45] + '... [See Man Page]'
    elif first_period >= 0:
        description = description[:first_period]
    
    # Escape some more things
    description = fish_escape_single_quote(description)
    escaped_cmd = fish_escape_single_quote(CMDNAME)
    
    output_complete_command(escaped_cmd, fish_options, description, built_command_output)

    

def removeGroffFormatting(data):
#    data = data.replace("\fI","")
#    data = data.replace("\fP","")
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
        add_diagnostic('Command is ' + CMDNAME)

        if options_matched == None:
            add_diagnostic('Unable to find options')
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
                    add_diagnostic(optionName + " doesn't contain - ")
#                    return False
                else:
                    optionName = unquoteDoubleQuotes(optionName)
                    optionName = unquoteSingleQuotes(optionName)
                    optionDescription = data[1].strip().replace("\n"," ")
#                    print >> sys.stderr, "Option: ", optionName," Description: ", optionDescription , '\n'
                    builtcommand(optionName, optionDescription)
                    
            else:
                add_diagnostic('Unable to split option from description')
                return False

            options_section = options_section[options_matched.end()-3:]
            options_matched = re.search(options_parts_regex, options_section)

    def fallback(self, options_section):
        add_diagnostic('Falling Back')
        options_parts_regex = re.compile("\.TP( \d+)?(.*?)\.TP", re.DOTALL)
        options_matched = re.search(options_parts_regex, options_section)
        if options_matched == None:
            add_diagnostic('Still not found')
            return False
        while options_matched != None:
            data = options_matched.group(2)
            data = removeGroffFormatting(data)
            data = data.strip()
            data = data.split("\n",1)
            if (len(data)>1 and len(data[1].strip())>0): # and len(data[1])<400):
                optionName = data[0].strip()
                if ( optionName.find("-") == -1):
                    add_diagnostic(optionName + "doesn't contains -")
                else:
                    optionName = unquoteDoubleQuotes(optionName)
                    optionName = unquoteSingleQuotes(optionName)
                    optionDescription = data[1].strip().replace("\n"," ")
#                    print "Option: ", optionName," Description: ", optionDescription , '\n'
                    builtcommand(optionName, optionDescription)
            else:
                add_diagnostic('Unable to split option from description')
                return False

            options_section = options_section[options_matched.end()-3:]
            options_matched = re.search(options_parts_regex, options_section)
        return True

    def fallback2(self, options_section):
        add_diagnostic('Falling Back2')
        ix_remover_regex = re.compile("\.IX.*")
        trailing_num_regex = re.compile('\\d+$')
        options_parts_regex = re.compile("\.IP (.*?)\.IP", re.DOTALL)

        options_section = re.sub(ix_remover_regex, "", options_section)
        options_matched = re.search(options_parts_regex, options_section)
        if options_matched == None:
            add_diagnostic('Still not found2')
            return False
        while options_matched != None:
            data = options_matched.group(1)
            
#            print "Data is : ", data
            data = removeGroffFormatting(data)
            data = data.strip()
            data = data.split("\n",1)
            if (len(data)>1 and len(data[1].strip())>0): # and len(data[1])<400):
#                print "Data[0] is: ", data[0]

#                data = re.sub(trailing_num_regex, "", data)
                optionName = re.sub(trailing_num_regex, "", data[0].strip())
                
                if ('-' not in optionName):
                    add_diagnostic(optionName + " doesn't contain -")
                else:
                    optionName = optionName.strip()
                    optionName = unquoteDoubleQuotes(optionName)
                    optionName = unquoteSingleQuotes(optionName)
                    optionDescription = data[1].strip().replace("\n"," ")
#                    print "Option: ", optionName," Description: ", optionDescription , '\n'
                    builtcommand(optionName, optionDescription)
            else:
#                print data
                add_diagnostic('Unable to split option from description')
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
        
#        if (options_section_matched == None):
#            print "Falling Back"
#            options_section_regex = re.compile( "\.SH OPTIONS(.*?)$", re.DOTALL)
#            options_section_matched = re.search( options_section_regex, manpage)
#        print manpage
        options_section = options_section_matched.group(1)
#        print options_section
        #   print options_section
        #   sys.exit(1)

#        options_parts_regex = re.compile("\.TP(.*?)\.TP", re.DOTALL)
        options_parts_regex = re.compile("\.[I|T]P( \d+(\.\d)?i?)?(.*?)\.[I|T]P", re.DOTALL)
#        options_parts_regex = re.compile("\.TP(.*?)[(\.TP)|(\.SH)]", re.DOTALL)
        options_matched = re.search(options_parts_regex, options_section)
        add_diagnostic('Command is ' + CMDNAME)

        if options_matched == None:
            add_diagnostic(self.name() + ': Unable to find options')
            return False

        while (options_matched != None):
            #       print len(options_matched.groups())
            data = options_matched.group(3)

            data = removeGroffFormatting(data)

            data = data.strip()

            data = data.split("\n",1)
#            print >> sys.stderr, data
            if (len(data)>1 and len(data[1].strip())>0): # and len(data[1])<400):
                optionName = data[0].strip()
                if '-' not in optionName:
                    add_diagnostic(optionName + " doesn't contain -")
                else:
                    optionName = unquoteDoubleQuotes(optionName)
                    optionName = unquoteSingleQuotes(optionName)
                    optionDescription = data[1].strip().replace("\n"," ")
#                    print "Option: ", optionName," Description: ", optionDescription , '\n'
                    builtcommand(optionName, optionDescription)
            else:
                # print >> sys.stderr, data
                add_diagnostic('Unable to split option from description')
                
#                return False

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
        add_diagnostic('Command is ' + CMDNAME)

        if options_matched == None:
            add_diagnostic('Unable to find options section')
            return False

        while (options_matched != None):
#            print len(options_matched.groups())
            data = options_matched.group(1)

            data = removeGroffFormatting(data)
            data = data.strip()
            data = data.split("\n",1)

            if (len(data)>1): # and len(data[1])<400):
                optionName = data[0].strip()
                if ( optionName.find("-") == -1):
                    add_diagnostic(optionName + "doesn't contain -")
                else:
                    optionName = unquoteDoubleQuotes(optionName)
                    optionName = unquoteSingleQuotes(optionName)
                    optionDescription = data[1].strip().replace("\n"," ")
#                    print >> sys.stderr, "Option: ", optionName," Description: ", optionDescription , '\n'
                    builtcommand(optionName, optionDescription)

            else:
                add_diagnostic('Unable to split option from description')
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
#                    print "Option: ", optionName," Description: ", optionDescription , '\n'
                    builtcommand(optionName, optionDescription)

            else:
                add_diagnostic('Unable to split option from description')
                return False

            options_section = options_section[options_matched.end()-3:]
            options_matched = re.search(options_parts_regex, options_section)

        return True

    def name(self):
        return "Type4"

class TypeDarwinManParser(ManParser):
    def isMyType(self, manpage):
        options_section_matched = compileAndSearch("\.S[hH] DESCRIPTION", manpage)
        return options_section_matched != None
    
    def trim_groff(self, line):
        # Remove initial period
        if line.startswith('.'):
            line = line[1:]
        # Skip leading groff crud
        while re.match('[A-Z][a-z]\s', line):
            line = line[3:]
        return line
    
    def is_option(self, line):
        return line.startswith('.It Fl')
    
    
    def parseManPage(self, manpage):
        got_something = False
        lines =  manpage.splitlines()
        # Discard lines until we get to ".sh Description"
        while lines and not (lines[0].startswith('.Sh DESCRIPTION') or lines[0].startswith('.SH DESCRIPTION')):
            lines.pop(0)
        
        while lines:
            # Pop until we get to the next option
            while lines and not self.is_option(lines[0]):
                lines.pop(0)
                
            if not lines:
                continue
            
            # Extract the name
            name = self.trim_groff(lines.pop(0)).strip()
            
            # Extract the description
            desc = ''
            while lines and not self.is_option(lines[0]):
                # print "*", lines[0]
                desc = desc + lines.pop(0)
            
            #print "name: ", name
            
            if name == '-':
                # Skip double -- arguments
                continue
            elif len(name) > 1:
                # Output the command
                builtcommand('--' + name, desc)
                got_something = True
            elif len(name) == 1:
                builtcommand('-' + name, desc)
                got_something = True
                
        return got_something
                
    def name(self):
        return "Darwin man parser"


class TypeDeroffManParser(ManParser):
    def isMyType(self, manpage):
        return True # We're optimists
    
    def is_option(self, line):
        return line.startswith('-')
        
    def could_be_description(self, line):
        return len(line) > 0 and not line.startswith('-')
    
    def parseManPage(self, manpage):
        d = Deroffer()
        d.deroff(manpage)
        output = d.get_output()
        lines = output.split('\n')
        
        got_something = False
        
        # Discard lines until we get to DESCRIPTION or OPTIONS
        while lines and not (lines[0].startswith('DESCRIPTION') or lines[0].startswith('OPTIONS') or lines[0].startswith('COMMAND OPTIONS')):
            lines.pop(0)
        
        while lines:
            # Pop until we get to the next option
            while lines and not self.is_option(lines[0]):
                lines.pop(0)    
            
            if not lines:
                continue
            
            options = lines.pop(0)
            
            # Pop until we get to either an empty line or a line starting with -
            description = ''
            while lines and self.could_be_description(lines[0]):
                if description: description += ' '
                description += lines.pop(0)
            
            builtcommand(options, description)
            got_something = True
            
        return got_something
            
                
    def name(self):
        return "Deroffing man parser"

def parse_manpage_at_path(manpage_path):
    filename = os.path.basename(manpage_path)

    # Clear diagnostics
    global diagnostic_indent
    diagnostic_output[:] = []
    diagnostic_indent = 0

    # Get the "base" command, e.g. gcc.1.gz -> gcc
    global CMDNAME
    CMDNAME = filename.split('.', 1)[0]

    # Set up some diagnostics
    add_diagnostic('Considering ' + manpage_path)
    diagnostic_indent += 1
    
    if manpage_path.endswith('.gz'):
        fd = gzip.open(manpage_path, 'r')
    else:
        fd = open(manpage_path, 'r')
    manpage = fd.read()
    fd.close()
    
    # Get the "base" command, e.g. gcc.1.gz -> gcc
    cmd_base = CMDNAME.split('.', 1)[0]
    ignoredcommands = ["cc", "g++", "gcc", "c++", "cpp", "emacs", "gprof", "wget", "ld", "awk"]
    if cmd_base in ignoredcommands:
        return
        
    # Ignore perl's gazillion man pages
    ignored_prefixes = ['perl', 'zsh']
    for prefix in ignored_prefixes:
        if cmd_base.startswith(prefix):
            return
            
    # Ignore the millions of links to BUILTIN(1)
    if manpage.find('BUILTIN 1') != -1:
        return
            
    
    # Clear the output list
    built_command_output[:] = []
        
    parsers = [Type1ManParser(), Type2ManParser(), Type4ManParser(), Type3ManParser(), TypeDarwinManParser(), TypeDeroffManParser()]
    parsersToTry = [p for p in parsers if p.isMyType(manpage)]
        
    success = False
    if not parsersToTry:
        add_diagnostic(manpage_path + ": Not supported")
    else:
        for parser in parsersToTry:
            add_diagnostic('Trying parser ' + parser.name())
            diagnostic_indent += 1
            success = parser.parseManPage(manpage)
            diagnostic_indent -= 1
            if success: break
        
        if success:
            built_command_output.insert(0, "# %s: %s" % (CMDNAME, parser.name()))
            for line in built_command_output:
                print line
            print ''
            add_diagnostic(manpage_path + ' parsed successfully')
        else:
            parser_names =  ', '.join(p.name() for p in parsersToTry)
            #add_diagnostic('%s contains no options or is unparsable' % manpage_path, True)
            add_diagnostic('%s contains no options or is unparsable (tried parser %s)' % (manpage_path, parser_names), True)
    return success

def compare_paths(a, b):
    """ Compare two paths by their base name, case insensitive """
    return cmp(os.path.basename(a).lower(), os.path.basename(b).lower())

def parse_and_output_man_pages(paths):
    global diagnostic_indent
    paths.sort(compare_paths)
    total_count = len(paths)
    successful_count = 0
    for manpage_path in paths:
        try:
            if parse_manpage_at_path(manpage_path):
                successful_count += 1
        except IOError:
            diagnostic_indent = 0
            add_diagnostic('Cannot open ' + manpage_path)
        except (KeyboardInterrupt, SystemExit):
            raise
        except:
            add_diagnostic('Error parsing %s: %s' % (manpage_path, sys.exc_info()[0]), True)
            flush_diagnostics(sys.stderr)
            traceback.print_exc(file=sys.stderr)
        flush_diagnostics(sys.stderr)
    add_diagnostic("Successfully parsed %d / %d pages" % (successful_count, total_count), True)
    flush_diagnostics(sys.stderr)
    

if __name__ == "__main__":
    args = sys.argv[1:]
    if args and (args[0] == '-v' or args[0] == '--verbose'):
        VERBOSE = True
        args.pop(0)
    
    if False:
        parse_and_output_man_pages(args)
    else:
        # Profiling code
        import cProfile, pstats
        cProfile.run('parse_and_output_man_pages(args)', 'fooprof')
        p = pstats.Stats('fooprof')
        p.sort_stats('cumulative').print_stats(100)
