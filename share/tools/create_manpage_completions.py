#!/usr/bin/env python
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

import string, sys, re, os.path, gzip, traceback, getopt, errno
from deroff import Deroffer

# Whether we're Python 3
IS_PY3 = sys.version_info[0] >= 3

# This gets set to the name of the command that we are currently executing
CMDNAME = ""

# Information used to track which of our parsers were successful
PARSER_INFO = {}

# builtcommand writes into this global variable, yuck
built_command_output = []

# Diagnostic output
diagnostic_output = []
diagnostic_indent = 0

# Three diagnostic verbosity levels
VERY_VERBOSE, BRIEF_VERBOSE, NOT_VERBOSE = 2, 1, 0

# Pick some reasonable default values for settings
global VERBOSITY, WRITE_TO_STDOUT, DEROFF_ONLY
VERBOSITY, WRITE_TO_STDOUT, DEROFF_ONLY = NOT_VERBOSE, False, False

def add_diagnostic(dgn, msg_verbosity = VERY_VERBOSE):
    # Add a diagnostic message, if msg_verbosity <= VERBOSITY
    if msg_verbosity <= VERBOSITY:
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
g_fish_safe_chars = frozenset(string.ascii_letters + string.digits + '_+-|/:=@~')

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
        option = option.strip()

        # Skip some problematic cases
        if option in ['-', '--']: continue

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

    # Here's what we'll use to truncate if necessary
    max_description_width = 63
    truncation_suffix = '… [See Man Page]'
    
    # Try to include as many whole sentences as will fit
    sentences = description.split('.')
    truncated_description = sentences[0] + '.'
    for line in sentences[1:]:
        if not line: continue
        proposed_description = truncated_description + ' ' + line + '.'
        if len(proposed_description) <= max_description_width:
            # It fits
            truncated_description = proposed_description
        else:
            # No fit
            break
    
    # If the first sentence does not fit, truncate if necessary
    if len(truncated_description) > max_description_width:
        prefix_len = max_description_width - len(truncation_suffix)
        truncated_description = truncated_description[:prefix_len] + truncation_suffix

    # Escape some more things
    truncated_description = fish_escape_single_quote(truncated_description)
    escaped_cmd = fish_escape_single_quote(CMDNAME)

    output_complete_command(escaped_cmd, fish_options, truncated_description, built_command_output)



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
        add_diagnostic('Command is ' + CMDNAME)

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
                    add_diagnostic(optionName + " doesn't contain - ")
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
            
        # If the line ends with a space and then a period or comma, then erase the space
        # This hack handles lines of the form '.Ar projectname .'
        if line.endswith(' ,') or line.endswith(' .'):
            line = line[:-2] + line[-1]
        return line

    def count_argument_dashes(self, line):
        # Determine how many dashes the line has using the following regex hack
        # Look for the start of a line, followed by a dot, then a sequence of
        # one or more dashes ('Fl')
        result = 0
        if line.startswith('.'):
            line = line[4:]
            while line.startswith('Fl '):
                result = result + 1
                line = line[3:]
        return result
            
        


    # Replace some groff escapes. There's a lot we don't bother to handle.
    def groff_replace_escapes(self, line):
        line = line.replace('.Nm', CMDNAME)
        line = line.replace('\\ ', ' ')
        line = line.replace('\& ', '')
        line = line.replace(r'.\"', '')
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

            # Get the line and clean it up
            line = lines.pop(0)
            
            # Try to guess how many dashes this argument has
            dash_count = self.count_argument_dashes(line)
                        
            line = self.groff_replace_escapes(line)
            line = self.trim_groff(line)
            line = line.strip()
            if not line: continue

            # Extract the name
            name = line.split(None, 2)[0]

            # Extract the description
            desc_lines = []
            while lines and not self.is_option(lines[0]):
                line = lines.pop(0).strip()
                if line.startswith('.'):
                    line = self.groff_replace_escapes(line)
                    line = self.trim_groff(line).strip()
                if line:
                    desc_lines.append(line)
            desc = ' '.join(desc_lines)

            # print "name: ", name
            # print "desc: ", desc

            if name == '-':
                # Skip double -- arguments
                continue
            elif len(name) > 1:
                # Output the command
                builtcommand(('-' * dash_count) + name, desc)
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

        # Look for BUGS and stop there
        for idx in range(len(lines)):
            line = lines[idx]
            if line.startswith('BUGS'):
                # Drop remaining elements
                lines[idx:] = []
                break

        while lines:
            # Pop until we get to the next option
            while lines and not self.is_option(lines[0]):
                line = lines.pop(0)

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

# Return whether the file at the given path is overwritable
# Raises IOError if it cannot be opened
def file_is_overwritable(path):
    result = False
    file = open(path, 'r')
    for line in file:
        # Skip leading empty lines
        line = line.strip()
        if not line:
            continue

        # We look in the initial run of lines that start with #
        if not line.startswith('#'):
            break

        # See if this contains the magic word
        if 'Autogenerated' in line:
            result = True
            break

    file.close()
    return result


# Return whether the file at the given path either does not exist, or exists but appears to be a file we output (and hence can overwrite)
def file_missing_or_overwritable(path):
    try:
        return file_is_overwritable(path)
    except IOError as err:
        if err.errno == 2:
            # File does not exist, full steam ahead
            return True
        else:
            # Something else happened
            return False

# Delete the file if it is autogenerated
def cleanup_autogenerated_file(path):
    try:
        if file_is_overwritable(path):
            os.remove(path)
    except (OSError, IOError):
        pass

def parse_manpage_at_path(manpage_path, yield_to_dirs, output_directory):
    filename = os.path.basename(manpage_path)

    # Clear diagnostics
    global diagnostic_indent
    diagnostic_output[:] = []
    diagnostic_indent = 0

    # Set up some diagnostics
    add_diagnostic('Considering ' + manpage_path)
    diagnostic_indent += 1

    if manpage_path.endswith('.gz'):
        fd = gzip.open(manpage_path, 'r')
        manpage = fd.read()
        if IS_PY3: manpage = manpage.decode('latin-1')
    else:
        if IS_PY3:
            fd = open(manpage_path, 'r', encoding='latin-1')
        else:
            fd = open(manpage_path, 'r')
        manpage = fd.read()
    fd.close()

    manpage = str(manpage)

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

    if DEROFF_ONLY:
        parsers = [TypeDeroffManParser()]
    else:
        parsers = [Type1ManParser(), Type2ManParser(), Type4ManParser(), Type3ManParser(), TypeDarwinManParser(), TypeDeroffManParser()]
    parsersToTry = [p for p in parsers if p.isMyType(manpage)]

    success = False
    if not parsersToTry:
        add_diagnostic(manpage_path + ": Not supported")
    else:
        for parser in parsersToTry:
            parser_name = parser.name()
            add_diagnostic('Trying parser ' + parser_name)
            diagnostic_indent += 1
            success = parser.parseManPage(manpage)
            diagnostic_indent -= 1
            if success:
                PARSER_INFO.setdefault(parser_name, []).append(CMDNAME)
                break

        if success:
            if WRITE_TO_STDOUT:
                output_file = sys.stdout
            else:
                fullpath = os.path.join(output_directory, CMDNAME + '.fish')
                try:
                    if file_missing_or_overwritable(fullpath):
                        output_file = open(fullpath, 'w')
                    else:
                        add_diagnostic("Not overwriting the file at '%s'" % fullpath)

                except IOError as err:
                    add_diagnostic("Unable to open file '%s': error(%d): %s" % (fullpath, err.errno, err.strerror))
                    return False

            built_command_output.insert(0, "# " + CMDNAME)

            # Output the magic word Autogenerated so we can tell if we can overwrite this
            built_command_output.insert(1, "# Autogenerated from man page " + manpage_path)
            built_command_output.insert(2, "# using " + parser_name)
            for line in built_command_output:
                output_file.write(line)
                output_file.write('\n')
            output_file.write('\n')
            add_diagnostic(manpage_path + ' parsed successfully')
            if output_file != sys.stdout:
                output_file.close()
        else:
            parser_names =  ', '.join(p.name() for p in parsersToTry)
            #add_diagnostic('%s contains no options or is unparsable' % manpage_path, BRIEF_VERBOSE)
            add_diagnostic('%s contains no options or is unparsable (tried parser %s)' % (manpage_path, parser_names), BRIEF_VERBOSE)
    return success

# Indicates whether the given filename has a presence in one of the yield-to directories
# If so, there's a bespoke completion and we should not generate one
def file_in_yield_directory(filename, yield_to_dirs):
    for yield_dir in yield_to_dirs:
        test_path = os.path.join(yield_dir, filename)
        if os.path.isfile(test_path):
            # Yield to the existing file
            return True
    return False

# Indicates whether we want to skip this command because it already had a non-autogenerated completion
def should_skip_man_page(output_path, filename, yield_to_dirs):
    # No reason to skip if we're writing to stdout
    if WRITE_TO_STDOUT:
        return false

    # Check all the yield directories
    for yield_dir in yield_to_dirs:
        test_path = os.path.join(yield_dir, filename)
        if os.path.isfile(test_path):
            # Yield to the existing file
            return true

    # See if there's a hand-written file already
    if not file_missing_or_overwritable(output_path):
        return true

    # We made it through, so don't skip
    return false



def parse_and_output_man_pages(paths, output_directory, yield_to_dirs, show_progress):
    global diagnostic_indent, CMDNAME
    paths.sort()
    total_count = len(paths)
    successful_count, index = 0, 0
    padding_len = len(str(total_count))
    last_progress_string_length = 0
    if show_progress and not WRITE_TO_STDOUT:
        print("Parsing man pages and writing completions to {0}".format(output_directory))
    for manpage_path in paths:
        index += 1

        # Get the "base" command, e.g. gcc.1.gz -> gcc
        man_file_name = os.path.basename(manpage_path)
        CMDNAME = man_file_name.split('.', 1)[0]
        output_file_name = CMDNAME + '.fish'

        # Show progress if we're doing that
        if show_progress:
            progress_str = '  {0} / {1} : {2}'.format((str(index).rjust(padding_len)), total_count, man_file_name)
            # Pad on the right with spaces so we overwrite whatever we wrote last time
            padded_progress_str = progress_str.ljust(last_progress_string_length)
            last_progress_string_length = len(progress_str)
            sys.stdout.write("\r{0}\r".format(padded_progress_str))
            sys.stdout.flush()

        # Maybe we want to skip this item
        skip = False
        if not WRITE_TO_STDOUT:
            # Compute the path that we would write to
            output_path = os.path.join(output_directory, output_file_name)

            if file_in_yield_directory(output_file_name, yield_to_dirs):
                # We're duplicating a bespoke completion - delete any existing completion
                skip = True
                cleanup_autogenerated_file(output_path)
            elif not file_missing_or_overwritable(output_path):
                # Don't overwrite a user-created completion
                skip = True

        # Now skip if requested
        if skip:
            continue

        try:
            if parse_manpage_at_path(manpage_path, yield_to_dirs, output_directory):
                successful_count += 1
        except IOError:
            diagnostic_indent = 0
            add_diagnostic('Cannot open ' + manpage_path)
        except (KeyboardInterrupt, SystemExit):
            raise
        except:
            add_diagnostic('Error parsing %s: %s' % (manpage_path, sys.exc_info()[0]), BRIEF_VERBOSE)
            flush_diagnostics(sys.stderr)
            traceback.print_exc(file=sys.stderr)
        flush_diagnostics(sys.stderr)
    print("") #Newline after loop
    add_diagnostic("Successfully parsed %d / %d pages" % (successful_count, total_count), BRIEF_VERBOSE)
    flush_diagnostics(sys.stderr)

def get_paths_from_manpath():
    # Return all the paths to man(1) files in the manpath
    import subprocess, os
    proc = subprocess.Popen(['manpath'], stdout=subprocess.PIPE)
    manpath, err_data = proc.communicate()
    parent_paths = manpath.decode().strip().split(':')
    if not parent_paths:
        sys.stderr.write("Unable to get the manpath (tried manpath)\n")
        sys.exit(-1)
    result = []
    for parent_path in parent_paths:
        directory_path = os.path.join(parent_path, 'man1')
        try:
            names = os.listdir(directory_path)
        except OSError as e:
            names = []
        names.sort()
        for name in names:
            result.append(os.path.join(directory_path, name))
    return result



def usage(script_name):
    print("Usage: {0} [-v, --verbose] [-s, --stdout] [-d, --directory] [-p, --progress] files...".format(script_name))
    print("""Command options are:
     -h, --help\t\tShow this help message
     -v, --verbose [0, 1, 2]\tShow debugging output to stderr. Larger is more verbose.
     -s, --stdout\tWrite all completions to stdout (trumps the --directory option)
     -d, --directory [dir]\tWrite all completions to the given directory, instead of to ~/.config/fish/completions
     -y, --yield-to [dir]\tSkip completions that are already present in the given directory
     -m, --manpath\tProcess all man1 files available in the manpath (as determined by manpath)
     -p, --progress\tShow progress
    """)

if __name__ == "__main__":
    script_name = sys.argv[0]
    try:
        opts, file_paths = getopt.gnu_getopt(sys.argv[1:], 'v:sd:hmpy:z', ['verbose=', 'stdout', 'directory=', 'help', 'manpath', 'progress', 'yield-to='])
    except getopt.GetoptError as err:
        print(err.msg) # will print something like "option -a not recognized"
        usage(script_name)
        sys.exit(2)

    # If a completion already exists in one of the yield-to directories, then don't overwrite it
    # And even delete an existing autogenerated one
    yield_to_dirs = []

    use_manpath, show_progress, custom_dir = False, False, False
    output_directory = ''
    for opt, value in opts:
        if opt in ('-v', '--verbose'):
            VERBOSITY = int(value)
        elif opt in ('-s', '--stdout'):
            WRITE_TO_STDOUT = True
        elif opt in ('-d', '--directory'):
            output_directory = value
        elif opt in ('-h', '--help'):
            usage(script_name)
            sys.exit(0)
        elif opt in ('-m', '--manpath'):
            use_manpath = True
        elif opt in ('-p', '--progress'):
            show_progress = True
        elif opt in ('-y', '--yield-to'):
            yield_to_dirs.append(value)
            if not os.path.isdir(value):
                sys.stderr.write("Warning: yield-to directory does not exist: '{0}'\n".format(value))
        elif opt in ('-z'):
            DEROFF_ONLY = True
        else:
            assert False, "unhandled option"

    if use_manpath:
        # Fetch all man1 files from the manpath
        file_paths.extend(get_paths_from_manpath())

    if not file_paths:
        print("No paths specified")
        sys.exit(0)

    if not WRITE_TO_STDOUT and not output_directory:
        # Default to ~/.config/fish/completions/
        # Create it if it doesn't exist
        output_directory = os.path.expanduser('~/.config/fish/completions/')
        try:
            os.makedirs(output_directory)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise

    if True:
        parse_and_output_man_pages(file_paths, output_directory, yield_to_dirs, show_progress)
    else:
        # Profiling code
        import cProfile, pstats
        cProfile.run('parse_and_output_man_pages(file_paths, output_directory, yield_to_dirs, show_progress)', 'fooprof')
        p = pstats.Stats('fooprof')
        p.sort_stats('cumulative').print_stats(100)

    # Here we can write out all the parser infos
    if False:
        for name in PARSER_INFO:
            print('Parser ' + name + ':')
            print('\t' + ', '.join(PARSER_INFO[name]))
            print('')