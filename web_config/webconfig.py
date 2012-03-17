#!/usr/bin/python

import SimpleHTTPServer
import SocketServer
import webbrowser
import subprocess
import re, json, socket, sys

def run_fish_cmd(text):
	from subprocess import PIPE
	p = subprocess.Popen(["fish"], stdin=PIPE, stdout=PIPE, stderr=PIPE)
	out, err = p.communicate(text)
	return out, err

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
		out, err = run_fish_cmd('set')
		for match in re.finditer(r"fish_color_(\S+) (.+)", out):
			color_name, color_value = match.group(1, 2)
			result.append([color_name.strip(), color_value.strip()])
		print result
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
		out, err = run_fish_cmd('set')
		
		# Put all the variables into a dictionary
		vars = {}
		for line in out.split('\n'):
			comps = line.split(' ', 1)
			if len(comps) < 2: continue
			fish_var = FishVar(comps[0], comps[1])
			vars[fish_var.name] = fish_var
			
		# Mark universal variables
		for name in self.do_get_variable_names('set -nU'):
			if name in vars: vars[name].universal = True
		# Mark exported variables
		for name in self.do_get_variable_names('set -nx'):
			if name in vars: vars[name].exported = True
		
		return [vars[key].get_json_obj() for key in sorted(vars.keys(), key=str.lower)]
		
		

	def do_get_color_for_variable(self, name):
		"Return the color with the given name, or the empty string if there is none"
		out, err = run_fish_cmd("echo -n $" + name)
		return out
				
	def do_GET(self):
		p = self.path
		if p == '/colors/':
			output = self.do_get_colors()
		elif p == '/functions/':
			output = self.do_get_functions()
		elif p == '/variables/':
			output = self.do_get_variables()
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
		print len(output)
		print output
		json.dump(output, self.wfile)
		
		

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
	print "Unable to start a web server"
	sys.exit(-1)

	
webbrowser.open("http://localhost:%d" % PORT)

print "serving at port", PORT
httpd.serve_forever()
