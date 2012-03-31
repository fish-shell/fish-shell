#!/usr/bin/python

import SimpleHTTPServer
import SocketServer
import webbrowser
import subprocess
import re, json, socket, os, sys, cgi, select

def run_fish_cmd(text):
	from subprocess import PIPE
	p = subprocess.Popen(["fish"], stdin=PIPE, stdout=PIPE, stderr=PIPE)
	out, err = p.communicate(text)
	return out, err

named_colors = {
	'black'   : '000000',
	'red'     : 'FF0000',
	'green'   : '00FF00',
	'brown'   : '725000',
	'yellow'  : 'FFFF00',
	'blue'    : '0000FF',
	'magenta' : 'FF00FF',
	'purple'  : 'FF00FF',
	'cyan'    : '00FFFF',
	'white'   : 'FFFFFF'
}

def parse_one_color(comp):
	""" A basic function to parse a single color value like 'FFA000' """
	if comp in named_colors:
		# Named color
		return named_colors[comp]
	elif re.match(r"[0-9a-fA-F]{3}", comp) is not None or re.match(r"[0-9a-fA-F]{6}", comp) is not None:
		# Hex color
		return comp
	else:
		# Unknown
		return ''


def parse_color(color_str):
	""" A basic function to parse a color string, for example, 'red' '--bold' """
	comps = color_str.split(' ')
	color = 'normal'
	background_color = ''
	bold, underline = False, False
	for comp in comps:
		# Remove quotes
		comp = comp.strip("'\" ")
		if comp == '--bold':
			bold = True
		elif comp == '--underline':
			underline = True
		elif comp.startswith('--background='):
			# Background color
			background_color = parse_one_color(comp[len('--background='):])
		else:
			# Regular color
			maybe_color = parse_one_color(comp)
			if maybe_color: color = maybe_color
	
	return [color, background_color, bold, underline]
	
	
def parse_bool(val):
	val = val.lower()
	if val.startswith('f') or val.startswith('0'): return False
	if val.startswith('t') or val.startswith('1'): return True
	return bool(val)

class FishVar:
	""" A class that represents a variable """
	def __init__(self, name, value):
		self.name = name
		self.value = value
		self.universal = False
		self.exported = False
		
	def get_json_obj(self):
		# Return an array(3): name, value, flags
		flags = []
		if self.universal: flags.append('universal')
		if self.exported: flags.append('exported')
		return [self.name, self.value, ', '.join(flags)]

class FishConfigHTTPRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
	
	def do_get_colors(self):
		"Look for fish_color_*"
		result = []
		remaining = set(['normal',
						 'error',
						 'command',
						 'end',
						 'param',
						 'comment',
						 'match',
						 'search_match',
						 'operator',
						 'escape',
						 'quote',
						 'redirection',
						 'valid_path',
						 'autosuggestion'
						 ])
    
		out, err = run_fish_cmd('set -L')
		for line in out.split('\n'):
			for match in re.finditer(r"^fish_color_(\S+) ?(.*)", line):
				color_name, color_value = [x.strip() for x in match.group(1, 2)]
				result.append([color_name, parse_color(color_value)])
				remaining.discard(color_name)
				
		# Ensure that we have all the color names we know about, so that if the
		# user deletes one he can still set it again via the web interface
		for x in remaining:
			result.append([x, parse_color('')])
			
		# Sort our result (by their keys)
		result.sort()
		
		return result
		
	def do_get_functions(self):
		out, err = run_fish_cmd('functions')
		out = out.strip()
		
		# Not sure why fish sometimes returns this with newlines
		if "\n" in out:
			return out.split('\n')
		else:
			return out.strip().split(', ')
	
	def do_get_variable_names(self, cmd):
		" Given a command like 'set -U' return all the variable names "
		out, err = run_fish_cmd(cmd)
		return out.split('\n')
	
	def do_get_variables(self):
		out, err = run_fish_cmd('set -L')
		
		# Put all the variables into a dictionary
		vars = {}
		for line in out.split('\n'):
			comps = line.split(' ', 1)
			if len(comps) < 2: continue
			fish_var = FishVar(comps[0], comps[1])
			vars[fish_var.name] = fish_var
			
		# Mark universal variables. L means don't abbreviate.
		for name in self.do_get_variable_names('set -nUL'):
			if name in vars: vars[name].universal = True
		# Mark exported variables. L means don't abbreviate.
		for name in self.do_get_variable_names('set -nxL'):
			if name in vars: vars[name].exported = True
		
		return [vars[key].get_json_obj() for key in sorted(vars.keys(), key=str.lower)]
	
	def do_get_history(self):
		# Use \x1e ("record separator") to distinguish between history items. The first
		# backslash is so Python passes one backslash to fish
		out, err = run_fish_cmd('for val in $history; echo -n $val \\x1e; end')
		return out.split('\x1e')
		

	def do_get_color_for_variable(self, name):
		"Return the color with the given name, or the empty string if there is none"
		out, err = run_fish_cmd("echo -n $" + name)
		return out
		
	def do_set_color_for_variable(self, name, color, background_color, bold, underline):
		if not color: color = 'normal'
		"Sets a color for a fish color name, like 'autosuggestion'"
		command = 'set -U fish_color_' + name
		if color: command += ' ' + color
		if background_color: command += ' --background=' + background_color
		if bold: command += ' --bold'
		if underline: command += ' --underline'
		
		out, err = run_fish_cmd(command)
		return out
		
	def do_get_function(self, func_name):
		out, err = run_fish_cmd('functions ' + func_name)
		return out
		
	def do_GET(self):
		p = self.path
		if p == '/colors/':
			output = self.do_get_colors()
		elif p == '/functions/':
			output = self.do_get_functions()
		elif p == '/variables/':
			output = self.do_get_variables()
		elif p == '/history/':
			output = self.do_get_history()
		elif re.match(r"/color/(\w+)/", p):
			name = re.match(r"/color/(\w+)/", p).group(1)
			output = self.do_get_color_for_variable(name)
		else:
			return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)
				
		# Return valid output
		self.send_response(200)
		self.send_header('Content-type','text/html')
		self.wfile.write('\n')
		
		# Output JSON
		json.dump(output, self.wfile)
		
	def do_POST(self):
		p = self.path
		ctype, pdict = cgi.parse_header(self.headers.getheader('content-type'))
		if ctype == 'multipart/form-data':
			postvars = cgi.parse_multipart(self.rfile, pdict)
		elif ctype == 'application/x-www-form-urlencoded':
			length = int(self.headers.getheader('content-length'))
			postvars = cgi.parse_qs(self.rfile.read(length), keep_blank_values=1)
		else:
			postvars = {}
		
		if p == '/set_color/':
			what = postvars.get('what')
			color = postvars.get('color')
			background_color = postvars.get('background_color')
			bold = postvars.get('bold')
			underline = postvars.get('underline')
			if what:
				# Not sure why we get lists here?
				output = self.do_set_color_for_variable(what[0], color[0], background_color[0], parse_bool(bold[0]), parse_bool(underline[0]))
			else:
				output = 'Bad request'
		elif p == '/get_function/':
			what = postvars.get('what')
			output = [self.do_get_function(what[0])]
		else:
			return SimpleHTTPServer.SimpleHTTPRequestHandler.do_POST(self)
			
		# Return valid output
		self.send_response(200)
		self.send_header('Content-type','text/html')
		self.wfile.write('\n')
		
		# Output JSON
		json.dump(output, self.wfile)

	def log_request(self, code='-', size='-'):
		""" Disable request logging """
		pass

# Make sure that the working directory is the one that contains the script server file,
# because the document root is the working directory
where = os.path.dirname(sys.argv[0])
os.chdir(where)

# Try to find a suitable port
PORT = 8000
while PORT <= 9000:
	try:
		Handler = FishConfigHTTPRequestHandler
		httpd = SocketServer.TCPServer(("", PORT), Handler)
		# Success
		break;
	except socket.error:
		type, value = sys.exc_info()[:2]
		if 'Address already in use' not in value:
			break
	PORT += 1

if PORT > 9000:
	# Nobody say it
	print "Unable to find an open port between 8000 and 9000"
	sys.exit(-1)

	
url = 'http://localhost:%d' % PORT

print "Web config started at '%s'. Hit enter to stop." % url
webbrowser.open(url)

# Select on stdin and httpd
stdin_no = sys.stdin.fileno()
while True:
	ready_read, _, _ = select.select([sys.stdin.fileno(), httpd.fileno()], [], [])
	if stdin_no in ready_read:
		print "Shutting down."
		# Consume the newline so it doesn't get printed by the caller
		sys.stdin.readline()
		break
	else:
		httpd.handle_request()
