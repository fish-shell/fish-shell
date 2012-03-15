#!/usr/bin/python

import SimpleHTTPServer
import SocketServer
import webbrowser
import subprocess
import re
import json

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
			result.append([color_name, color_value])
		print result
		return result

	def do_GET(self):
		p = self.path
		if p == '/colors/':
			output = self.do_get_colors()
		else:
			return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)
				
		# Return valid output
		self.send_response(200)
		self.send_header('Content-type','text/html')
		self.wfile.write('\n')
		
		# Output JSON
		json.dump(output, self.wfile)
		
		

PORT = 8011
Handler = FishConfigHTTPRequestHandler
httpd = SocketServer.TCPServer(("", PORT), Handler)

webbrowser.open("http://localhost:%d" % PORT)

print "serving at port", PORT
httpd.serve_forever()
