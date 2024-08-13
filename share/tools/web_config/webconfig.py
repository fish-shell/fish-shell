from __future__ import unicode_literals
from __future__ import print_function
import binascii

try:
    from html import escape as escape_html
except ImportError:
    from cgi import escape as escape_html
import errno
import glob
import multiprocessing.pool
import operator
import os
import platform
import re
import select
import socket
import subprocess
import sys
import tempfile
import threading
from itertools import chain

COMMON_WSL_CMD_PATHS = (
    "/mnt/c/Windows/System32",
    "/windir/c/Windows/System32",
    "/c/Windows/System32",
)
FISH_BIN_PATH = False  # will be set later
IS_PY2 = sys.version_info[0] == 2

if IS_PY2:
    import SimpleHTTPServer
    import SocketServer
    from urlparse import parse_qs
else:
    import http.server as SimpleHTTPServer
    import socketserver as SocketServer
    from urllib.parse import parse_qs

try:
    import json
except ImportError:
    import simplejson as json


# Disable CLI web browsers
term = os.environ.pop("TERM", None)
# This import must be done with an empty $TERM, otherwise a command-line browser may be started
# which will block the whole process - see https://docs.python.org/3/library/webbrowser.html
import webbrowser

if term:
    os.environ["TERM"] = term


def find_executable(exe, paths=()):
    final_path = os.environ["PATH"].split(os.pathsep)
    if paths:
        final_path.extend(paths)
    for p in final_path:
        proposed_path = os.path.join(p, exe)
        if os.access(proposed_path, os.X_OK):
            return proposed_path


def isMacOS10_12_5_OrLater():
    """Return whether this system is macOS 10.12.5 or a later version."""
    try:
        return [int(x) for x in platform.mac_ver()[0].split(".")] >= [10, 12, 5]
    except ValueError:
        return False


def is_wsl():
    """Return whether we are running under the Windows Subsystem for Linux"""
    if "linux" in platform.system().lower() and os.access("/proc/version", os.R_OK):
        with open("/proc/version", "r") as f:
            # Find 'Microsoft' for wsl1 and 'microsoft' for wsl2
            if "microsoft" in f.read().lower():
                return True
    return False


def is_windows():
    """Return whether we are running under the Windows"""
    return sys.platform.startswith("win")


def is_sailfish_os():
    """Return whether we are running on Sailfish OS"""
    if "linux" in platform.system().lower() and os.access(
        "/etc/sailfish-release", os.R_OK
    ):
        with open("/etc/sailfish-release", "r") as f:
            # Find 'ID=sailfishos'
            if "sailfishos" in f.read():
                return True
    return False


def is_termux():
    """Return whether we are running under the Termux application for Android"""
    return "com.termux" in os.environ["PATH"] and find_executable("termux-open-url")


def is_chromeos_garcon():
    """Return whether we are running in Chrome OS and the browser can't see local files"""
    # In Crostini Chrome OS Linux, the default browser opens URLs in Chrome
    # running outside the linux VM. This browser does not have access to the
    # Linux filesystem. This uses Garcon, see for example
    # https://chromium.googlesource.com/chromiumos/platform2/+/master/vm_tools/garcon/#opening-urls
    # https://source.chromium.org/search?q=garcon-url-handler
    try:
        return "garcon-url-handler" in webbrowser.get().name
    except:
        return False


def run_fish_cmd(text):
    # Ensure that fish is using UTF-8.
    ctype = os.environ.get("LC_ALL", os.environ.get("LC_CTYPE", os.environ.get("LANG")))
    env = None
    if ctype is None or re.search(r"\.utf-?8$", ctype, flags=re.I) is None:
        # override LC_CTYPE with en_US.UTF-8
        # We're assuming this locale exists.
        # Fish makes the same assumption in config.fish
        env = os.environ.copy()
        env.update(LC_CTYPE="en_US.UTF-8", LANG="en_US.UTF-8")

    print("$ " + text)
    p = subprocess.Popen(
        [FISH_BIN_PATH],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
    )
    out, err = p.communicate(text.encode("utf-8"))
    out = out.decode("utf-8", "replace")
    err = err.decode("utf-8", "replace")
    return out, err


def escape_fish_cmd(text):
    # Replace one backslash with two, and single quotes with backslash-quote
    escaped = text.replace("\\", "\\\\").replace("'", "\\'")
    return "'" + escaped + "'"


def strip_one_layer(text, char):
    # Strip the text from one layer of a given character
    if text[-1] == char:
        text = text[:-1]
    if text[0] == char:
        text = text[1:]
    return text


named_colors = {
    "black": "000000",
    "red": "800000",
    "green": "008000",
    "brown": "725000",
    "yellow": "808000",
    "blue": "000080",
    "magenta": "800080",
    "purple": "800080",
    "cyan": "008080",
    "grey": "e5e5e5",
    "brgrey": "555555",
    "white": "c0c0c0",
    "brblack": "808080",
    "brred": "ff0000",
    "brgreen": "00ff00",
    "brbrown": "ffff00",
    "bryellow": "ffff00",
    "brblue": "0000ff",
    "brmagenta": "ff00ff",
    "brpurple": "ff00ff",
    "brcyan": "00ffff",
    "brwhite": "ffffff",
}

bindings_blacklist = set(["self-insert", "'begin;end'"])


def parse_one_color(comp):
    """A basic function to parse a single color value like 'FFA000'"""
    if comp in named_colors:
        # Named color
        return named_colors[comp]
    elif (
        re.match(r"[0-9a-fA-F]{3}", comp) is not None
        or re.match(r"[0-9a-fA-F]{6}", comp) is not None
    ):
        # Hex color
        return comp
    else:
        # Unknown
        return ""


def better_color(c1, c2):
    """Indicate which color is "better", i.e. prefer term256 colors"""
    if not c2:
        return c1
    if not c1:
        return c2
    if c1 == "normal":
        return c2
    if c2 == "normal":
        return c1
    if c2 in named_colors:
        return c1
    if c1 in named_colors:
        return c2
    return c1


def parse_color(color_str):
    """A basic function to parse a color string, for example, 'red' '--bold'."""
    comps = color_str.split(" ")
    color = ""
    background_color = ""
    bold, underline, italics, dim, reverse = False, False, False, False, False
    for comp in comps:
        # Remove quotes
        comp = comp.strip("'\" ")
        if comp == "--bold" or comp == "-o":
            bold = True
        elif comp == "--underline" or comp == "-u":
            underline = True
        elif comp == "--italics" or comp == "-i":
            italics = True
        elif comp == "--dim" or comp == "-d":
            dim = True
        elif comp == "--reverse" or comp == "-r":
            reverse = True
        elif comp.startswith("--background"):
            # Background color
            c = comp[len("--background=") :]
            parsed_c = parse_one_color(c)
            # We prefer the unparsed version - if it says "brgreen", we use brgreen,
            # instead of 00ff00
            if better_color(background_color, parsed_c) == parsed_c:
                background_color = c
        elif comp.startswith("-b"):
            # Background color in short.
            skip = len("-b")
            if comp[len("-b=")] in ["=", " "]:
                skip += 1
            c = comp[skip:]
            parsed_c = parse_one_color(c)
            if better_color(background_color, parsed_c) == parsed_c:
                background_color = c
        else:
            # Regular color
            parsed_c = parse_one_color(comp)
            if better_color(color, parsed_c) == parsed_c:
                color = comp

    return {
        "color": color,
        "background": background_color,
        "bold": bold,
        "underline": underline,
        "italics": italics,
        "dim": dim,
        "reverse": reverse,
    }


def unparse_color(col):
    """A basic function to return the fish version of a color dict"""
    if isinstance(col, str):
        return col
    ret = ""
    if col["color"]:
        ret += col["color"]
    if col["bold"]:
        ret += " --bold"
    if col["underline"]:
        ret += " --underline"
    if col["italics"]:
        ret += " --italics"
    if col["dim"]:
        ret += " --dim"
    if col["reverse"]:
        ret += " --reverse"
    if col["background"]:
        ret += " --background=" + col["background"]
    return ret


def parse_bool(val):
    val = val.lower()
    if val.startswith("f") or val.startswith("0"):
        return False
    if val.startswith("t") or val.startswith("1"):
        return True
    return bool(val)


def html_color_for_ansi_color_index(val):
    arr = [
        "black",
        "#FF0000",
        "#00FF00",
        "#AA5500",
        "#0000FF",
        "#AA00AA",
        "#00AAAA",
        "#AAAAAA",
        "#555555",
        "#FF5555",
        "#55FF55",
        "#FFFF55",
        "#5555FF",
        "#FF55FF",
        "#55FFFF",
        "white",
        "#000000",
        "#00005f",
        "#000087",
        "#0000af",
        "#0000d7",
        "#0000ff",
        "#005f00",
        "#005f5f",
        "#005f87",
        "#005faf",
        "#005fd7",
        "#005fff",
        "#008700",
        "#00875f",
        "#008787",
        "#0087af",
        "#0087d7",
        "#0087ff",
        "#00af00",
        "#00af5f",
        "#00af87",
        "#00afaf",
        "#00afd7",
        "#00afff",
        "#00d700",
        "#00d75f",
        "#00d787",
        "#00d7af",
        "#00d7d7",
        "#00d7ff",
        "#00ff00",
        "#00ff5f",
        "#00ff87",
        "#00ffaf",
        "#00ffd7",
        "#00ffff",
        "#5f0000",
        "#5f005f",
        "#5f0087",
        "#5f00af",
        "#5f00d7",
        "#5f00ff",
        "#5f5f00",
        "#5f5f5f",
        "#5f5f87",
        "#5f5faf",
        "#5f5fd7",
        "#5f5fff",
        "#5f8700",
        "#5f875f",
        "#5f8787",
        "#5f87af",
        "#5f87d7",
        "#5f87ff",
        "#5faf00",
        "#5faf5f",
        "#5faf87",
        "#5fafaf",
        "#5fafd7",
        "#5fafff",
        "#5fd700",
        "#5fd75f",
        "#5fd787",
        "#5fd7af",
        "#5fd7d7",
        "#5fd7ff",
        "#5fff00",
        "#5fff5f",
        "#5fff87",
        "#5fffaf",
        "#5fffd7",
        "#5fffff",
        "#870000",
        "#87005f",
        "#870087",
        "#8700af",
        "#8700d7",
        "#8700ff",
        "#875f00",
        "#875f5f",
        "#875f87",
        "#875faf",
        "#875fd7",
        "#875fff",
        "#878700",
        "#87875f",
        "#878787",
        "#8787af",
        "#8787d7",
        "#8787ff",
        "#87af00",
        "#87af5f",
        "#87af87",
        "#87afaf",
        "#87afd7",
        "#87afff",
        "#87d700",
        "#87d75f",
        "#87d787",
        "#87d7af",
        "#87d7d7",
        "#87d7ff",
        "#87ff00",
        "#87ff5f",
        "#87ff87",
        "#87ffaf",
        "#87ffd7",
        "#87ffff",
        "#af0000",
        "#af005f",
        "#af0087",
        "#af00af",
        "#af00d7",
        "#af00ff",
        "#af5f00",
        "#af5f5f",
        "#af5f87",
        "#af5faf",
        "#af5fd7",
        "#af5fff",
        "#af8700",
        "#af875f",
        "#af8787",
        "#af87af",
        "#af87d7",
        "#af87ff",
        "#afaf00",
        "#afaf5f",
        "#afaf87",
        "#afafaf",
        "#afafd7",
        "#afafff",
        "#afd700",
        "#afd75f",
        "#afd787",
        "#afd7af",
        "#afd7d7",
        "#afd7ff",
        "#afff00",
        "#afff5f",
        "#afff87",
        "#afffaf",
        "#afffd7",
        "#afffff",
        "#d70000",
        "#d7005f",
        "#d70087",
        "#d700af",
        "#d700d7",
        "#d700ff",
        "#d75f00",
        "#d75f5f",
        "#d75f87",
        "#d75faf",
        "#d75fd7",
        "#d75fff",
        "#d78700",
        "#d7875f",
        "#d78787",
        "#d787af",
        "#d787d7",
        "#d787ff",
        "#d7af00",
        "#d7af5f",
        "#d7af87",
        "#d7afaf",
        "#d7afd7",
        "#d7afff",
        "#d7d700",
        "#d7d75f",
        "#d7d787",
        "#d7d7af",
        "#d7d7d7",
        "#d7d7ff",
        "#d7ff00",
        "#d7ff5f",
        "#d7ff87",
        "#d7ffaf",
        "#d7ffd7",
        "#d7ffff",
        "#ff0000",
        "#ff005f",
        "#ff0087",
        "#ff00af",
        "#ff00d7",
        "#ff00ff",
        "#ff5f00",
        "#ff5f5f",
        "#ff5f87",
        "#ff5faf",
        "#ff5fd7",
        "#ff5fff",
        "#ff8700",
        "#ff875f",
        "#ff8787",
        "#ff87af",
        "#ff87d7",
        "#ff87ff",
        "#ffaf00",
        "#ffaf5f",
        "#ffaf87",
        "#ffafaf",
        "#ffafd7",
        "#ffafff",
        "#ffd700",
        "#ffd75f",
        "#ffd787",
        "#ffd7af",
        "#ffd7d7",
        "#ffd7ff",
        "#ffff00",
        "#ffff5f",
        "#ffff87",
        "#ffffaf",
        "#ffffd7",
        "#ffffff",
        "#080808",
        "#121212",
        "#1c1c1c",
        "#262626",
        "#303030",
        "#3a3a3a",
        "#444444",
        "#4e4e4e",
        "#585858",
        "#626262",
        "#6c6c6c",
        "#767676",
        "#808080",
        "#8a8a8a",
        "#949494",
        "#9e9e9e",
        "#a8a8a8",
        "#b2b2b2",
        "#bcbcbc",
        "#c6c6c6",
        "#d0d0d0",
        "#dadada",
        "#e4e4e4",
        "#eeeeee",
    ]
    if val < 0 or val >= len(arr):
        return ""
    else:
        return arr[val]


# Function to return special ANSI escapes like exit_attribute_mode
g_special_escapes_dict = None


def get_special_ansi_escapes():
    global g_special_escapes_dict
    if g_special_escapes_dict is None:
        try:
            import curses

            g_special_escapes_dict = {}
            curses.setupterm()

            # Helper function to get a value for a tparm
            def get_tparm(key):
                val = None
                key = curses.tigetstr(key)
                if key:
                    val = curses.tparm(key)
                if val:
                    val = val.decode("utf-8")
                # Use an empty string instead of None.
                return "" if val is None else val

            # Just a few for now
            g_special_escapes_dict["exit_attribute_mode"] = get_tparm("sgr0")
            g_special_escapes_dict["bold"] = get_tparm("bold")
            g_special_escapes_dict["underline"] = get_tparm("smul")
        except ImportError:
            print("WARNING: The python curses module is missing.")
            print("WARNING: Falling back to xterm-256color settings.")
            print("WARNING: Rebuild python with curses headers!")

            g_special_escapes_dict = {
                "exit_attribute_mode": "\x1b(B\x1b[m",
                "bold": "\x1b[1m",
                "underline": "\x1b[4m",
            }

    return g_special_escapes_dict


# Given a known ANSI escape sequence, convert it to HTML and append to the list
# Returns whether we have an open <span>


def append_html_for_ansi_escape(full_val, result, span_open):
    # Strip off the initial \x1b[ and terminating m
    val = full_val[2:-1]

    # Helper function to close a span if it's open
    def close_span():
        if span_open:
            result.append("</span>")

    # term24bit foreground color
    match = re.match(r"38;2;(\d+);(\d+);(\d+)", val)
    if match is not None:
        close_span()
        # Just use the rgb values directly
        html_color = "#%02x%02x%02x" % (
            int(match.group(1)),
            int(match.group(2)),
            int(match.group(3)),
        )
        result.append('<span style="color: ' + html_color + '">')
        return True  # span now open

    # term256 foreground color
    match = re.match(r"38;5;(\d+)", val)
    if match is not None:
        close_span()
        html_color = html_color_for_ansi_color_index(int(match.group(1)))
        result.append('<span style="color: ' + html_color + '">')
        return True  # span now open

    # term16 foreground color
    if val in (str(x) for x in chain(range(90, 97), range(30, 38))):
        close_span()
        html_color = html_color_for_ansi_color_index(
            int(val) - (30 if int(val) < 90 else 82)
        )
        result.append('<span style="color: ' + html_color + '">')
        return True  # span now open

    # Try special escapes
    special_escapes = get_special_ansi_escapes()
    if full_val == special_escapes["exit_attribute_mode"]:
        close_span()
        return False

    # TODO We don't handle bold, underline, italics, dim, or reverse yet

    # Do nothing on failure
    return span_open


def strip_ansi(val):
    # Make a half-assed effort to strip ANSI control sequences
    # We assume that all such sequences start with 0x1b and end with m or ctrl-o,
    # which catches most cases
    return re.sub("\x1b[^m]*m\x0f?", "", val)


def ansi_prompt_line_width(val):
    # Given an ANSI prompt, return the length of its longest line, as in the
    # number of characters it takes up. Start by stripping off ANSI.
    stripped_val = strip_ansi(val)

    # Now count the longest line
    return max([len(x) for x in stripped_val.split("\n")])


def ansi_to_html(val):
    # Split us up by ANSI escape sequences. We want to catch not only the
    # standard color codes, but also things like sgr0. Hence this lame check.
    # Note that Python 2.6 doesn't have a flag param to re.split, so we have to
    # compile it first.
    reg = re.compile(
        """
        (                        # Capture
         \x1b                    # Escape
         [^m]*                   # Zero or more non-'m's
         m                       # Literal m terminates the sequence
         \x0f?                   # HACK: A ctrl-o - this is how tmux' sgr0 ends
        )                        # End capture
        """,
        re.VERBOSE,
    )
    separated = reg.split(val)

    # We have to HTML escape the text and convert ANSI escapes into HTML
    # Collect it all into this array
    result = []

    span_open = False

    # Text is at even indexes, escape sequences at odd indexes
    for i in range(len(separated)):
        component = separated[i]
        if i % 2 == 0:
            # It's text, possibly empty
            # Clean up other ANSI junk
            result.append(escape_html(strip_ansi(component)))
        else:
            # It's an escape sequence. Close the previous escape.
            span_open = append_html_for_ansi_escape(component, result, span_open)

    # Close final escape
    if span_open:
        result.append("</span>")

    # Remove empty elements
    result = [x for x in result if x]

    # Clean up empty spans, the nasty way
    idx = len(result) - 1
    while idx >= 1:
        if result[idx] == "</span>" and result[idx - 1].startswith("<span"):
            # Empty span, delete these two
            result[idx - 1 : idx + 1] = []
            idx = idx - 1
        idx = idx - 1

    return "".join(result)


class FishVar:
    """A class that represents a variable"""

    def __init__(self, name, value):
        self.name = name
        self.value = value
        self.universal = False
        self.exported = False

    def get_json_obj(self):
        # Return an array(3): name, value, flags
        flags = []
        if self.universal:
            flags.append("universal")
        if self.exported:
            flags.append("exported")
        return {"name": self.name, "value": self.value, "Flags": ", ".join(flags)}


class FishBinding:
    """A class that represents keyboard binding"""

    def __init__(self, command, raw_binding, readable_binding, description=None):
        self.command = command
        self.bindings = []
        self.description = description
        self.add_binding(raw_binding, readable_binding)

    def add_binding(self, raw_binding, readable_binding):
        for i in self.bindings:
            if i["readable_binding"] == readable_binding:
                i["raw_bindings"].append(raw_binding)
                break
        else:
            self.bindings.append(
                {"readable_binding": readable_binding, "raw_bindings": [raw_binding]}
            )

    def get_json_obj(self):
        return {
            "command": self.command,
            "bindings": self.bindings,
            "description": self.description,
        }


class BindingParser:
    """Class to parse codes for bind command"""

    # TODO: What does snext and sprevious mean ?
    readable_keys = {
        "dc": "Delete",
        "npage": "Page Up",
        "ppage": "Page Down",
        "sdc": "Shift Delete",
        "shome": "Shift Home",
        "left": "Left Arrow",
        "right": "Right Arrow",
        "up": "Up Arrow",
        "down": "Down Arrow",
        "sleft": "Shift Left",
        "sright": "Shift Right",
        "btab": "Shift Tab",
    }

    def set_buffer(self, buffer):
        """Sets code to parse"""

        self.buffer = buffer or b""
        self.index = 0

    def get_char(self):
        """Gets next character from buffer"""
        if self.index >= len(self.buffer):
            return "\0"
        c = self.buffer[self.index]
        self.index += 1
        return c

    def unget_char(self):
        """Goes back by one character for parsing"""

        self.index -= 1

    def end(self):
        """Returns true if reached end of buffer"""

        return self.index >= len(self.buffer)

    def parse_control_sequence(self):
        """Parses terminal specifiec control sequences"""

        result = ""
        c = self.get_char()

        # \e0 is used to denote start of control sequence
        if c == "O":
            c = self.get_char()

        # \[1\; is start of control sequence
        if c == "1":
            b = self.get_char()
            c = self.get_char()
            if b == "\\" and c == "~":
                result += "Home"
            elif c == ";":
                c = self.get_char()

        # 3 is Alt
        if c == "3":
            result += "ALT - "
            c = self.get_char()

        # \[4\~ is End
        if c == "4":
            b = self.get_char()
            c = self.get_char()
            if b == "\\" and c == "~":
                result += "End"

        # 5 is Ctrl
        if c == "5":
            result += "CTRL - "
            c = self.get_char()

        # 9 is Alt
        if c == "9":
            result += "ALT - "
            c = self.get_char()

        if c == "A":
            result += "Up Arrow"
        elif c == "B":
            result += "Down Arrow"
        elif c == "C":
            result += "Right Arrow"
        elif c == "D":
            result += "Left Arrow"
        elif c == "F":
            result += "End"
        elif c == "H":
            result += "Home"

        return result

    def get_readable_binding(self):
        """Gets a readable representation of binding"""

        try:
            result = BindingParser.readable_keys[self.buffer.lower()]
        except KeyError:
            result = self.parse_binding()

        return result

    def parse_binding(self):
        readable_command = ""
        result = ""
        alt = ctrl = False

        while not self.end():
            c = self.get_char()

            if c == "\\":
                c = self.get_char()
                if c == "e":
                    d = self.get_char()
                    if d == "O":
                        self.unget_char()
                        result += self.parse_control_sequence()
                    elif d == "\\":
                        if self.get_char() == "[":
                            result += self.parse_control_sequence()
                        else:
                            self.unget_char()
                            self.unget_char()
                            alt = True
                    elif d == "\0":
                        result += "ESC"
                    else:
                        alt = True
                        self.unget_char()
                elif c == "c":
                    ctrl = True
                elif c == "n":
                    result += "Enter"
                elif c == "t":
                    result += "Tab"
                elif c == "b":
                    result += "Backspace"
                elif c.isalpha():
                    result += "\\" + c
                else:
                    result += c
            elif c == "\x7f":
                result += "Backspace"
            else:
                result += c
        if ctrl:
            readable_command += "CTRL - "
        if alt:
            readable_command += "ALT - "

        if result == "":
            return "unknown-control-sequence"

        return readable_command + result


class FishConfigTCPServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
    """TCPServer that only accepts connections from localhost (IPv4/IPv6)."""

    WHITELIST = set(["::1", "::ffff:127.0.0.1", "127.0.0.1"])

    address_family = socket.AF_INET6 if socket.has_ipv6 else socket.AF_INET

    def verify_request(self, request, client_address):
        return client_address[0] in FishConfigTCPServer.WHITELIST


class FishConfigHTTPRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    def write_to_wfile(self, txt):
        self.wfile.write(txt.encode("utf-8"))

    def do_get_colors(self, path=None):
        """Read the colors from a .theme file in path, or the current shell if no path has been given"""
        # Looks for fish_color_*.
        # Returns an array of lists [color_name, color_description, color_value]
        result = []

        # Make sure we return at least these
        remaining = set(
            [
                "normal",
                "error",
                "command",
                "end",
                "param",
                "comment",
                "match",
                "selection",
                "search_match",
                "operator",
                "escape",
                "quote",
                "redirection",
                "valid_path",
                "autosuggestion",
                "user",
                "host",
                "cancel",
            ]
        )

        # Here are our color descriptions
        descriptions = {
            "normal": "Default text",
            "command": "Ordinary commands",
            "quote": "Text within quotes",
            "redirection": "Like | and >",
            "end": "Like ; and &",
            "error": "Potential errors",
            "param": "Command parameters",
            "comment": "Comments start with #",
            "match": "Matching parenthesis",
            "selection": "Selected text",
            "search_match": "History searching",
            "history_current": "Directory history",
            "operator": "Like * and ~",
            "escape": "Escapes like \\n",
            "cwd": "Current directory",
            "cwd_root": "cwd for root user",
            "valid_path": "Valid paths",
            "autosuggestion": "Suggested completion",
            "user": "Username in the prompt",
            "host": "Hostname in the prompt",
            "cancel": "The ^C cancel indicator",
        }

        # If we don't have a path, we get the current theme.
        if not path:
            out, err = run_fish_cmd("set -L")
        else:
            with open(path) as f:
                out = f.read()
        extrainfo = {}
        for line in out.split("\n"):
            # Ignore empty lines
            if not line:
                continue
            # Lines starting with "#" can contain metadata.
            if line.startswith("#"):
                if not ":" in line:
                    continue
                key, value = line.split(":", maxsplit=1)
                key = key.strip("# '")
                value = value.strip(" '\"")
                # Only use keys we know
                if not key in ("name", "preferred_background", "url"):
                    continue
                if key == "preferred_background":
                    if value not in named_colors and not value.startswith("#"):
                        value = "#" + value
                extrainfo[key] = value

            for match in re.finditer(r"^fish_(pager_)?color_(\S+) ?(.*)", line):
                color_name, color_value = [x.strip() for x in match.group(2, 3)]
                if match.group(1):
                    color_name = "fish_pager_color_" + color_name
                color_desc = descriptions.get(color_name, "")
                data = {"name": color_name, "description": color_desc}
                data.update(parse_color(color_value))
                result.append(data)
                remaining.discard(color_name)

        # Sort our result (by their keys)
        result.sort(key=operator.itemgetter("name"))

        # Ensure that we have all the color names we know about, so that if the
        # user deletes one he can still set it again via the web interface
        for color_name in remaining:
            color_desc = descriptions.get(color_name, "")
            result.append([color_name, color_desc, parse_color("")])

        return result, extrainfo

    def do_get_functions(self):
        out, err = run_fish_cmd("functions")
        out = out.strip()

        # Not sure why fish sometimes returns this with newlines
        if "\n" in out:
            return out.split("\n")
        else:
            return out.strip().split(", ")

    def do_get_variable_names(self, cmd):
        "Given a command like 'set -U' return all the variable names"
        out, err = run_fish_cmd(cmd)
        return out.split("\n")

    def do_get_variables(self):
        out, err = run_fish_cmd("set -L")

        # Put all the variables into a dictionary
        vars = {}
        for line in out.split("\n"):
            comps = line.split(" ", 1)
            if len(comps) < 2:
                continue
            fish_var = FishVar(comps[0], comps[1])
            vars[fish_var.name] = fish_var

        # Mark universal variables. L means don't abbreviate.
        for name in self.do_get_variable_names("set -nUL"):
            if name in vars:
                vars[name].universal = True
        # Mark exported variables. L means don't abbreviate.
        for name in self.do_get_variable_names("set -nxL"):
            if name in vars:
                vars[name].exported = True

        # Do not return history as a variable, it may be so large the browser hangs.
        vars.pop("history", None)

        return [
            vars[key].get_json_obj()
            for key in sorted(vars.keys(), key=lambda x: x.lower())
        ]

    def do_get_bindings(self):
        """Get key bindings"""

        # Running __fish_config_interactive print fish greeting and
        # loads key bindings
        greeting, err = run_fish_cmd(" __fish_config_interactive")

        # Load the key bindings and then list them with bind
        out, err = run_fish_cmd("__fish_config_interactive; bind")

        # Remove fish greeting from output
        out = out[len(greeting) :]

        # Put all the bindings into a list
        bindings = []
        command_to_binding = {}
        binding_parser = BindingParser()

        for line in out.split("\n"):
            comps = line.split(" ", 2)

            # If we don't have "bind", a sequence and a mapping,
            # it's not a valid binding.
            if len(comps) < 3:
                continue

            # Store the "--preset" value for later
            if comps[1] == "--preset":
                preset = True
                # There's possibly a way to do this faster, but it's not important.
                comps = line.split(" ", 3)[1:]
            elif comps[1] == "--user":
                preset = False
                comps = line.split(" ", 3)[1:]
            # Check again if we removed the level.
            if len(comps) < 3:
                continue

            if comps[1] == "-k":
                key_name, command = comps[2].split(" ", 1)
                binding_parser.set_buffer(key_name.capitalize())
            else:
                key_name = None
                command = comps[2]
                binding_parser.set_buffer(comps[1])

            if command in bindings_blacklist:
                continue

            readable_binding = binding_parser.get_readable_binding()
            if command in command_to_binding:
                fish_binding = command_to_binding[command]
                fish_binding.add_binding(line, readable_binding)
            else:
                fish_binding = FishBinding(command, line, readable_binding)
                bindings.append(fish_binding)
                command_to_binding[command] = fish_binding

        return [binding.get_json_obj() for binding in bindings]

    def do_get_history(self):
        # Use NUL to distinguish between history items.
        out, err = run_fish_cmd("builtin history -z")
        result = out.split("\0")
        if result:
            result.pop()  # trim off the trailing element
        return result

    def do_get_color_for_variable(self, name):
        "Return the color with the given name, or the empty string if there is none."
        out, err = run_fish_cmd("echo -n $" + name)
        return out

    def do_set_color_for_variable(self, name, color):
        "Sets a color for a fish color name, like 'autosuggestion'"
        if not name:
            raise ValueError
        if not color and not color == "":
            color = "normal"
        else:
            color = unparse_color(color)
        if not name.startswith("fish_pager_color_"):
            varname = "fish_color_" + name
        # If the name already starts with "fish_", use it as the varname
        # This is needed for 'fish_pager_color' vars.
        if name.startswith("fish_"):
            varname = name
        # Check if the varname is allowable.
        varname = varname.strip()
        if not re.match("^[a-zA-Z0-9_]+$", varname):
            print("Refusing to use variable name: '", varname, "'")
            return
        color = color.strip()
        if not re.match("^[a-zA-Z0-9_= -]*$", color):
            print("Refusing to use color value: ", color)
            return
        command = "set -U " + varname
        command += " " + color

        out, err = run_fish_cmd(command)
        return out

    def do_get_function(self, func_name):
        out, err = run_fish_cmd("functions " + func_name + " | fish_indent --html")
        return out

    def do_delete_history_item(self, history_item_text):
        # It's really lame that we always return success here
        cmd = (
            "builtin history delete --case-sensitive --exact -- %s; builtin history save"
            % escape_fish_cmd(history_item_text)
        )
        out, err = run_fish_cmd(cmd)
        return True

    def do_set_prompt_function(self, prompt_func):
        cmd = (
            "functions -e fish_right_prompt; "
            + prompt_func
            + "\n"
            + "funcsave fish_prompt && funcsave fish_right_prompt 2>/dev/null"
        )
        out, err = run_fish_cmd(cmd)
        return len(err) == 0

    def do_get_prompt(self, prompt_function_text, extras_dict):
        # Return the prompt output by the given command
        cmd = (
            prompt_function_text
            + '\n builtin cd "'
            + initial_wd
            + '" \n false \n fish_prompt\n'
        )
        prompt_demo_ansi, err = run_fish_cmd(cmd)
        prompt_demo_html = ansi_to_html(prompt_demo_ansi)
        right_demo_ansi, err = run_fish_cmd(
            "functions -e fish_right_prompt; "
            + prompt_function_text
            + '\n builtin cd "'
            + initial_wd
            + '" \n false \n functions -q fish_right_prompt && fish_right_prompt\n'
        )
        right_demo_html = ansi_to_html(right_demo_ansi)
        prompt_demo_font_size = self.font_size_for_ansi_prompt(
            prompt_demo_ansi + right_demo_ansi
        )
        result = {
            "function": prompt_function_text,
            "demo": prompt_demo_html,
            "font_size": prompt_demo_font_size,
            "right": right_demo_html,
        }
        if extras_dict:
            result.update(extras_dict)
        return result

    def do_get_current_prompt(self):
        # Return the current prompt. We run 'false' to demonstrate how the
        # prompt shows the command status (#1624).
        prompt_func, err = run_fish_cmd(
            "functions fish_prompt; functions fish_right_prompt"
        )
        result = self.do_get_prompt(
            prompt_func.strip(),
            {"name": "Current"},
        )
        return result

    def do_get_sample_prompt(self, text, extras_dict):
        # Return the prompt you get from the given text. Extras_dict is a
        # dictionary whose values get merged in. We run 'false' to demonstrate
        # how the prompt shows the command status (#1624)
        return self.do_get_prompt(text.strip(), extras_dict)

    def parse_one_sample_prompt_hash(self, line, result_dict):
        # Allow us to skip whitespace, etc.
        if not line:
            return True
        if line.isspace():
            return True

        # Parse a comment hash like '# name: Classic'
        match = re.match(r"#\s*(\w+?): (.+)", line, re.IGNORECASE)
        if match:
            key = match.group(1).strip()
            value = match.group(2).strip()
            result_dict[key] = value
            return True
        # Skip other hash comments
        return line.startswith("#")

    def read_one_sample_prompt(self, path):
        try:
            with open(path, "rb") as fd:
                extras_dict = {}
                # Read one sample prompt from fd
                function_lines = []
                parsing_hashes = True
                unicode_lines = (line.decode("utf-8") for line in fd)
                for line in unicode_lines:
                    # Parse hashes until parse_one_sample_prompt_hash return
                    # False.
                    if parsing_hashes:
                        parsing_hashes = self.parse_one_sample_prompt_hash(
                            line, extras_dict
                        )
                    # Maybe not we're not parsing hashes, or maybe we already
                    # were not.
                    if not parsing_hashes:
                        function_lines.append(line)
                func = "".join(function_lines).strip()
                result = self.do_get_sample_prompt(func, extras_dict)
                return result
        except IOError:
            # Ignore unreadable files, etc.
            return None

    def do_get_sample_prompts_list(self):
        paths = sorted(glob.iglob("sample_prompts/*.fish"))
        result = []
        try:
            pool = multiprocessing.pool.ThreadPool(processes=8)

            # Kick off the "Current" meta-sample
            current_metasample_async = pool.apply_async(self.do_get_current_prompt)

            # Read all of the prompts in sample_prompts
            sample_results = pool.map(self.read_one_sample_prompt, paths, 1)
            result.append(current_metasample_async.get())
            result.extend([r for r in sample_results if r])
        except ImportError:
            # If the platform doesn't support multiprocessing, we just do it one at a time.
            # This happens e.g. on Termux.
            print(
                "Platform doesn't support multiprocessing, running one at a time. This may take a while."
            )
            result.append(self.do_get_current_prompt())
            result.extend([self.read_one_sample_prompt(path) for path in paths])
        return result

    def secure_startswith(self, haystack, needle):
        if len(haystack) < len(needle):
            return False
        bits = 0
        for x, y in zip(haystack, needle):
            bits |= ord(x) ^ ord(y)
        return bits == 0

    def font_size_for_ansi_prompt(self, prompt_demo_ansi):
        width = ansi_prompt_line_width(prompt_demo_ansi)
        # Pick a font size
        if width >= 70:
            font_size = "8pt"
        if width >= 60:
            font_size = "10pt"
        elif width >= 50:
            font_size = "11pt"
        elif width >= 40:
            font_size = "13pt"
        elif width >= 30:
            font_size = "15pt"
        elif width >= 25:
            font_size = "16pt"
        elif width >= 20:
            font_size = "17pt"
        else:
            font_size = "18pt"
        return font_size

    def do_GET(self):
        p = self.path

        authpath = "/" + authkey
        if self.secure_startswith(p, authpath):
            p = p[len(authpath) :]
        else:
            return self.send_error(403)
        self.path = p

        if p == "/colors/":
            # Construct our colorschemes.
            # Add the current scheme first, then the default.
            # The rest in alphabetical order.
            curcolors, curinfo = self.do_get_colors()
            defcolors, definfo = self.do_get_colors("themes/fish default.theme")
            curinfo.update({"theme": "Current", "colors": curcolors})
            definfo.update({"theme": "fish default", "colors": defcolors})
            output = [curinfo, definfo]

            confighome = (
                os.environ["XDG_CONFIG_HOME"]
                if "XDG_CONFIG_HOME" in os.environ
                else os.path.expanduser("~")
            )
            paths = list(glob.iglob(os.path.join(confighome, "fish", "themes/*.theme")))
            paths.extend(list(glob.iglob("themes/*.theme")))
            paths.sort(key=str.casefold)

            for p in paths:
                theme = os.path.splitext(os.path.basename(p))[0]
                if any(theme == d["theme"] for d in output):
                    continue
                out, outinfo = self.do_get_colors(p)
                outinfo.update({"theme": theme, "colors": out})
                output.append(outinfo)
        elif p == "/functions/":
            output = self.do_get_functions()
        elif p == "/variables/":
            output = self.do_get_variables()
        elif p == "/history/":
            # start = time.time()
            output = self.do_get_history()
            # end = time.time()
            # print "History: ", end - start
        elif p == "/sample_prompts/":
            output = self.do_get_sample_prompts_list()
        elif re.match(r"/color/(\w+)/", p):
            name = re.match(r"/color/(\w+)/", p).group(1)
            output = self.do_get_color_for_variable(name)
        elif p == "/bindings/":
            output = self.do_get_bindings()
        else:
            return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

        # Return valid output
        self.send_response(200)
        self.send_header("Content-type", "application/json")
        self.end_headers()
        self.write_to_wfile("\n")

        # Output JSON
        self.write_to_wfile(json.dumps(output))

    def do_POST(self):
        p = self.path

        authpath = "/" + authkey
        if self.secure_startswith(p, authpath):
            p = p[len(authpath) :]
        else:
            return self.send_error(403)
        self.path = p

        # This is cheesy, we want just the actual content-type.
        # In some cases it'll give us the encoding as well,
        # ("application/json;charset=utf-8")
        # but we don't currently care.
        ctype = self.headers["content-type"].split(";")[0]

        if ctype == "application/x-www-form-urlencoded":
            length = int(self.headers["content-length"])
            url_str = self.rfile.read(length).decode("utf-8")
            postvars = parse_qs(url_str, keep_blank_values=1)
        elif ctype == "application/json":
            length = int(self.headers["content-length"])
            # This used to use the provided encoding, but we use utf-8
            # all around the place and nobody has ever complained.
            #
            # If any other encoding is received this will raise a UnicodeError,
            # which will throw us out of the function and should at most exit webconfig.
            # If that happens to anyone we expect bug reports.
            url_str = self.rfile.read(length).decode("utf-8")
            postvars = json.loads(url_str)
        elif ctype == "multipart/form-data":
            # This used to be a thing, as far as I could find there's
            # no use anymore, but let's keep an error around just in case.
            return self.send_error(500)
        else:
            postvars = {}

        if p == "/set_color/":
            print("# Colorscheme: " + postvars.get("theme"))
            have_colors = set()
            known_colors = set(
                (
                    "fish_color_normal",
                    "fish_color_command",
                    "fish_color_keyword",
                    "fish_color_quote",
                    "fish_color_redirection",
                    "fish_color_end",
                    "fish_color_error",
                    "fish_color_param",
                    "fish_color_option",
                    "fish_color_comment",
                    "fish_color_selection",
                    "fish_color_operator",
                    "fish_color_escape",
                    "fish_color_autosuggestion",
                    "fish_color_cwd",
                    "fish_color_user",
                    "fish_color_host",
                    "fish_color_host_remote",
                    "fish_color_cancel",
                    "fish_color_search_match",
                    "fish_pager_color_progress",
                    "fish_pager_color_background",
                    "fish_pager_color_prefix",
                    "fish_pager_color_completion",
                    "fish_pager_color_description",
                    "fish_pager_color_selected_background",
                    "fish_pager_color_selected_prefix",
                    "fish_pager_color_selected_completion",
                    "fish_pager_color_selected_description",
                    "fish_pager_color_secondary_background",
                    "fish_pager_color_secondary_prefix",
                    "fish_pager_color_secondary_completion",
                    "fish_pager_color_secondary_description",
                )
            )
            output = ""
            for item in postvars.get("colors"):
                what = item.get("what")
                color = item.get("color")

                if what:
                    if not what.startswith("fish_pager_color_") and not what.startswith(
                        "fish_color_"
                    ):
                        have_colors.add("fish_color_" + what)
                    else:
                        have_colors.add(what)
                    output = self.do_set_color_for_variable(what, color)

            # Set all known colors that weren't defined in this theme
            # to empty, to avoid keeping around coloration from an earlier theme.
            for what in known_colors - have_colors:
                output += "\n" + self.do_set_color_for_variable(what, "")

        elif p == "/get_function/":
            what = postvars.get("what")
            output = [self.do_get_function(what[0])]
        elif p == "/delete_history_item/":
            what = postvars.get("what")
            if self.do_delete_history_item(what[0]):
                output = ["OK"]
            else:
                output = ["Unable to delete history item"]
        elif p == "/set_prompt/":
            what = postvars.get("fish_prompt")
            if self.do_set_prompt_function(what):
                output = ["OK"]
            else:
                output = ["Unable to set prompt"]
        else:
            return self.send_error(404)

        # Return valid output
        self.send_response(200)
        self.send_header("Content-type", "application/json")
        self.end_headers()
        self.write_to_wfile("\n")

        # Output JSON
        self.write_to_wfile(json.dumps(output))

    def log_request(self, code="-", size="-"):
        """Disable request logging"""
        pass

    def log_error(self, format, *args):
        if format == "code %d, message %s" and hasattr(self, "path"):
            # This appears to be a send_error() message
            # We want to include the path (if we have one)
            (code, msg) = args
            format = "code %d, message %s, path %s"
            args = (code, msg, self.path)
        SimpleHTTPServer.SimpleHTTPRequestHandler.log_error(self, format, *args)


redirect_template_html = """
<!DOCTYPE html>
<html>
 <head>
  <meta http-equiv="refresh" content="0;URL='%s'" />
 </head>
 <body>
  <p><a href="%s">Start the Fish Web config</a></p>
 </body>
</html>
"""

# find fish
fish_bin_dir = os.environ.get("__fish_bin_dir")
fish_bin_path = None

# only need the '.exe' extension on Windows
fish_bin_name = "fish.exe" if is_windows() else "fish"

if not fish_bin_dir:
    print("The $__fish_bin_dir environment variable is not set. " "Looking in $PATH...")
    fish_bin_path = find_executable(fish_bin_name)
    if not fish_bin_path:
        print("fish could not be found. Is fish installed correctly?")
        sys.exit(-1)
    else:
        print("fish found at '%s'" % fish_bin_path)

else:
    fish_bin_path = os.path.join(fish_bin_dir, fish_bin_name)

if not os.access(fish_bin_path, os.X_OK):
    print(
        "fish could not be executed at path '%s'. "
        "Is fish installed correctly?" % fish_bin_path
    )
    sys.exit(-1)
FISH_BIN_PATH = fish_bin_path

# We want to show the demo prompts in the directory from which this was invoked,
# so get the current working directory
initial_wd = os.getcwd()

# Make sure that the working directory is the one that contains the script
# server file, because the document root is the working directory.
where = os.path.dirname(sys.argv[0])
os.chdir(where)

# Generate a 16-byte random key as a hexadecimal string
authkey = binascii.b2a_hex(os.urandom(16)).decode("ascii")

# Try to find a suitable port
PORT = 8000
HOST = "::" if socket.has_ipv6 else "localhost"
while PORT <= 9000:
    try:
        Handler = FishConfigHTTPRequestHandler
        httpd = FishConfigTCPServer((HOST, PORT), Handler)
        # Success
        break
    except (OSError, socket.error) as e:
        if e.errno == errno.EAFNOSUPPORT and HOST == "::":
            print("Ipv6 seems to be unavailable, trying ipv4")
            # We can't do ipv6, let's try v4.
            HOST = "127.0.0.1"
            PORT = 8000
            # Override the class address_family variable (yes, this is a hack)
            FishConfigTCPServer.address_family = socket.AF_INET
            continue
        if e.errno != errno.EADDRINUSE:
            print(str(e))
            sys.exit(-1)
    PORT += 1

if PORT > 9000:
    # Nobody say it
    print("Unable to find an open port between 8000 and 9000")
    sys.exit(-1)

# Get any initial tab (functions, colors, etc)
# Just look at the first letter
initial_tab = ""
if len(sys.argv) > 1:
    for tab in [
        "functions",
        "prompt",
        "colors",
        "variables",
        "history",
        "bindings",
    ]:
        if tab.startswith(sys.argv[1]):
            initial_tab = "#!/" + tab
            break

url = "http://localhost:%d/%s/%s" % (PORT, authkey, initial_tab)

# Create temporary file to hold redirect to real server. This prevents exposing
# the URL containing the authentication key on the command line (see
# CVE-2014-2914 or https://github.com/fish-shell/fish-shell/issues/1438).
#

# If on Windows, the file needs to be closed after writing, otherwise, the browser won't be able to open it."
# unfortunately this was added in python 3.12, so we don't add it on other platforms
# to support older python versions there.
kwargs = {}
if is_windows():
    kwargs["delete_on_close"] = False
f = tempfile.NamedTemporaryFile(
    prefix="web_config",
    suffix=".html",
    mode="w",
    delete=True,
    **kwargs,
)

f.write(redirect_template_html % (url, url))
f.flush()

if is_windows():
    f.close()

# Open temporary file as URL
# Use open on macOS >= 10.12.5 to work around #4035.
fileurl = "file://" + f.name

if is_windows():
    fileurl = fileurl.replace("\\", "/")

esc = get_special_ansi_escapes()
print(
    "Web config started at %s%s%s"
    % (esc["underline"], fileurl, esc["exit_attribute_mode"])
)
print(
    "If that doesn't work, try opening %s%s%s"
    % (esc["underline"], url, esc["exit_attribute_mode"])
)
print("%sHit ENTER to stop.%s" % (esc["bold"], esc["exit_attribute_mode"]))


def runThing():
    if isMacOS10_12_5_OrLater():
        subprocess.check_call(["open", fileurl])
    elif is_wsl():
        cmd_path = find_executable("cmd.exe", COMMON_WSL_CMD_PATHS)
        if cmd_path:
            subprocess.call([cmd_path, "/c", "start %s" % url])
        else:
            print("Please add the directory containing cmd.exe to your $PATH")
            sys.exit(-1)
    elif is_termux():
        subprocess.call(["termux-open-url", url])
    elif is_chromeos_garcon():
        webbrowser.open(url)
    elif is_sailfish_os():
        subprocess.call(["xdg-open", url])
    else:
        webbrowser.open(fileurl)


# Some browsers still block webbrowser.open if they haven't been opened before,
# so we just spawn it in a thread.
thread = threading.Thread(target=runThing)
thread.start()

# Safari will open sockets and not write to them, causing potential hangs
# on shutdown.
httpd.block_on_close = False
httpd.daemon_threads = True

# Select on stdin and httpd
stdin_no = sys.stdin.fileno()


def create_socket(start_port, end_port):
    """Attempt to create a socket from a range of ports."""
    for port in range(start_port, end_port + 1):
        try:
            sck = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sck.bind(("localhost", port))
            sck.listen()
            return (sck, port)
        except socket.error:
            pass
    print("Unable to find an open port between {} and {}.".format(start_port, end_port))
    sys.exit(-1)


def capture_enter(port):
    """Read keyboard events and establish a socket connection when pressing the Enter."""
    import msvcrt

    while True:
        if msvcrt.kbhit():
            key = msvcrt.getch().decode()
            if key == "\r" or key == "\n":
                break
    sck = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sck.connect(("localhost", port))


def get_windows_signal():
    """Using socket as a replacement for stdin on Windows."""
    (sig, sig_port) = create_socket(8000, 9000)
    threading.Thread(target=capture_enter, args=(sig_port,)).start()
    return sig


try:
    httpd_fileno = httpd.fileno()
    sig = get_windows_signal() if is_windows() else sys.stdin
    sig_fileno = sig.fileno()
    while True:
        ready_read = select.select([sig_fileno, httpd_fileno], [], [])
        if ready_read[0][0] != httpd_fileno:
            print("Shutting down.")

            # On windows the newline has already been consumed by the capture_enter function.
            if not is_windows():
                # Consume the newline so it doesn't get printed by the caller
                sys.stdin.readline()
            break
        else:
            httpd.handle_request()
except KeyboardInterrupt:
    print("\nShutting down.")

# Clean up temporary file
# If on Windows, the file already closed
if not is_windows():
    f.close()

thread.join()
