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
		

	def do_GET(self):
		p = self.path
		if p == '/colors/':
			output = self.do_get_colors()
		elif p == '/functions/':
			output = self.do_get_functions()
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
