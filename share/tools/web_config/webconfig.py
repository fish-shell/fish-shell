#!/usr/bin/env python

# Whether we're Python 2
import sys
IS_PY2 = sys.version_info[0] == 2

if IS_PY2:
    import SimpleHTTPServer
    import SocketServer
    from urlparse import parse_qs
else:
    import http.server as SimpleHTTPServer
    import socketserver as SocketServer
    from urllib.parse import parse_qs
import webbrowser
import subprocess
import re, json, socket, os, sys, cgi, select, time

def run_fish_cmd(text):
    from subprocess import PIPE
    p = subprocess.Popen(["fish"], stdin=PIPE, stdout=PIPE, stderr=PIPE)
    if IS_PY2:
        out, err = p.communicate(text)
    else:
        out, err = p.communicate(bytes(text, 'utf-8'))
        out = str(out, 'utf-8')
        err = str(err, 'utf-8')
    return(out, err)

def escape_fish_cmd(text):
    # Replace one backslash with two, and single quotes with backslash-quote
    escaped = text.replace('\\', '\\\\').replace("'", "\\'")
    return "'" + escaped + "'"

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

def better_color(c1, c2):
    """ Indicate which color is "better", i.e. prefer term256 colors """
    if not c2: return c1
    if not c1: return c2
    if c1 == 'normal': return c2
    if c2 == 'normal': return c1
    if c2 in named_colors: return c1
    if c1 in named_colors: return c2
    return c1


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
            background_color = better_color(background_color, parse_one_color(comp[len('--background='):]))
        else:
            # Regular color
            color = better_color(color, parse_one_color(comp))

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

    def write_to_wfile(self, txt):
        if IS_PY2:
            self.wfile.write(txt)
        else: # Python 3
            self.wfile.write(bytes(txt, 'utf-8'))


    def do_get_colors(self):
        # Looks for fish_color_*.
        # Returns an array of lists [color_name, color_description, color_value]
        result = []

        # Make sure we return at least these
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

        # Here are our color descriptions
        descriptions = {
            'normal': 'Default text',
            'command': 'Ordinary commands',
            'quote': 'Text within quotes',
            'redirection': 'Like | and >',
            'end': 'Like ; and &',
            'error': 'Potential errors',
            'param': 'Command parameters',
            'comment': 'Comments start with #',
            'match': 'Matching parenthesis',
            'search_match': 'History searching',
            'history_current': 'Directory history',
            'operator': 'Like * and ~',
            'escape': 'Escapes like \\n',
            'cwd': 'Current directory',
            'cwd_root': 'cwd for root user',
            'valid_path': 'Valid paths',
            'autosuggestion': 'Suggested completion'
        }

        out, err = run_fish_cmd('set -L')
        for line in out.split('\n'):

            for match in re.finditer(r"^fish_color_(\S+) ?(.*)", line):
                color_name, color_value = [x.strip() for x in match.group(1, 2)]
                color_desc = descriptions.get(color_name, '')
                result.append([color_name, color_desc, parse_color(color_value)])
                remaining.discard(color_name)

        # Ensure that we have all the color names we know about, so that if the
        # user deletes one he can still set it again via the web interface
        for color_name in remaining:
            color_desc = descriptions.get(color_name, '')
            result.append([color_name, color_desc, parse_color('')])

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
        result = out.split(' \x1e')
        if result: result.pop() # Trim off the trailing element
        return result


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

    def do_delete_history_item(self, history_item_text):
        # It's really lame that we always return success here
        out, err = run_fish_cmd('builtin history --save --delete -- ' + escape_fish_cmd(history_item_text))
        return True

    def do_GET(self):
        p = self.path
        if p == '/colors/':
            output = self.do_get_colors()
        elif p == '/functions/':
            output = self.do_get_functions()
        elif p == '/variables/':
            output = self.do_get_variables()
        elif p == '/history/':
            # start = time.time()
            output = self.do_get_history()
            # end = time.time()
            # print "History: ", end - start
        elif re.match(r"/color/(\w+)/", p):
            name = re.match(r"/color/(\w+)/", p).group(1)
            output = self.do_get_color_for_variable(name)
        else:
            return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

        # Return valid output
        self.send_response(200)
        self.send_header('Content-type','text/html')
        self.write_to_wfile('\n')

        # Output JSON
        self.write_to_wfile(json.dumps(output))

    def do_POST(self):
        p = self.path
        if IS_PY2:
            ctype, pdict = cgi.parse_header(self.headers.getheader('content-type'))
        else: # Python 3
            ctype, pdict = cgi.parse_header(self.headers['content-type'])

        if ctype == 'multipart/form-data':
            postvars = cgi.parse_multipart(self.rfile, pdict)
        elif ctype == 'application/x-www-form-urlencoded':
            try:
                length = int(self.headers.getheader('content-length'))
            except AttributeError:
                length = int(self.headers['content-length'])

            data = self.rfile.read(length)
            if not IS_PY2:
                data = data.decode('utf-8')
            postvars = parse_qs(data, keep_blank_values=1)
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
        elif p == '/delete_history_item/':
            what = postvars.get('what')
            if self.do_delete_history_item(what[0]):
                output = ["OK"]
            else:
                output = ["Unable to delete history item"]
        else:
            return SimpleHTTPServer.SimpleHTTPRequestHandler.do_POST(self)

        # Return valid output
        self.send_response(200)
        self.send_header('Content-type','text/html')
        self.write_to_wfile('\n')

        # Output JSON
        self.write_to_wfile(json.dumps(output))

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
        err_type, err_value = sys.exc_info()[:2]
        # str(err_value) handles Python3 correctly
        if 'Address already in use' not in str(err_value):
            break
    PORT += 1

if PORT > 9000:
    # Nobody say it
    print("Unable to find an open port between 8000 and 9000")
    sys.exit(-1)

# Get any initial tab (functions, colors, etc)
# Just look at the first letter
initial_tab = ''
if len(sys.argv) > 1:
    for tab in ['functions', 'colors', 'variables', 'history']:
        if tab.startswith(sys.argv[1]):
            initial_tab = '#' + tab
            break

url = 'http://localhost:{0}/{1}'.format(PORT, initial_tab)
print("Web config started at '{0}'. Hit enter to stop.".format(url))
webbrowser.open(url)

# Select on stdin and httpd
stdin_no = sys.stdin.fileno()
while True:
    ready_read = select.select([sys.stdin.fileno(), httpd.fileno()], [], [])
    if ready_read[0][0] < 1:
        print("Shutting down.")
        # Consume the newline so it doesn't get printed by the caller
        sys.stdin.readline()
        break
    else:
        httpd.handle_request()
