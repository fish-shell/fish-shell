#!/usr/bin/python
# -*- coding: utf-8 -*-

import sys


class Deroffer:
    
    def __init__(self):
        self.reg_table = {}
        self.tr = range(256)
        self.nls = 2
        self.specletter = False
        self.pretty = False
        self.refer = False
        self.macro = 0
        self.nobody = False
        self.inlist = False
        self.inheader = False
        self.pic = False
        self.tbl = False
        self.tblstate = 0
        self.tblTab = ''
        self.eqn = False
        self.skipheaders = False
        self.skiplists = False
        self.ignore_sonx = False
        self.output = []
        
        self.OPTIONS = 0
        self.FORMAT = 1
        self.DATA = 2
        
        # words is uninteresting and should be treated as false
    
    def flush_output(self, where):
        if where:
            where.write(''.join(self.output))
        self.output[:] = []
    
    def get_output(self):
        return ''.join(self.output)
    
    def putchar(self, c):
        self.output.append(c)
        return c
        
    def condputchar(self, c):
        ci = ord(c)
        ci_trans = self.tr[ci & 0xFF]
        c_trans = chr(ci_trans)
        if not self.pic and not self.eqn and not self.refer and not self.macro and (not self.skiplists or not self.inlist) and (not self.skipheaders or not self.inheader):
            if self.pretty:
                if c == '\n':
                    self.nls += 1
                    if self.nls > 2: return c
                else:
                    self.nls = 0
                return self.putchar(c_trans)
            else:
                return self.putchar(c_trans)
        elif not self.pretty and c == '\n':
            return self.putchar(c_trans)
        else:
            return c
            
    def condputs(self, str):
        for c in str:
            self.condputchar(c)
    
    def str_at(self, idx):
        return self.s[idx:idx+1]
    
    def skip_char(self, amt=1):
        self.s = self.s[amt:]
        
    def skip_leading_whitespace(self):
        self.s = self.s.lstrip()
        
    def is_white(self, idx):
        # Note this returns false for empty strings (idx >= len(self.s))
        return self.str_at(idx).isspace()

    def prefix_at(self, offset, other_str):
        return self.s[offset:].startswith(other_str)
    
    def str_eq(offset, other, len):
        return self.s[offset:offset+len] == other[:len]

    def prch(self, idx):
        ch = self.str_at(idx)
        return ch and ch not in ' \t\n'

    def font(self):
        if self.str_at(0) == '\\' and self.str_at(1) == 'f':
            if self.str_at(2) == '(' and self.prch(3) and self.prch(4):
                self.skip_char(5)
                return True
            elif self.str_at(2) == '[':
                self.skip_char(2)
                while self.prch(0) and self.str_at(0) != ']': self.skip_char()
                if self.str_at(0) == ']': self.skip_char()
            elif self.prch(2):
                self.skip_char(3)
                return True
        return False
            
    def comment(self):
        if self.str_at(0) == '\\' and self.str_at(1) == '"':
            while self.str_at(0) and self.str_at(0) != '\n': self.skip_char()
            return True
        return False

    def numreq(self):
        if self.str_at(0) == '\\' and self.str_at(1) in 'hvwud' and self.str_at(2) == '\'':
            self.macro += 1
            self.skip_char(3)
            while self.str_at(0) != '\'' and self.esc_char():
                pass # Weird
            if self.str_at(0) == '\'':
                self.skip_char()
            self.macro -= 1
            return True
        return False

    def var(self):
        reg = ''
        if self.s.startswith('\\n'):
            if self.s[3:5].startswith('dy'):
                self.skip_char(5)
                return True
            elif self.str_at(2) == '(' and self.prch(3) and self.prch(4):
                self.skip_char(5)
                return True
            elif self.str_at(2) == '[' and self.prch(3):
                self.skip_char(3)
                while self.str_at(0) and self.str_at(0) != ']':
                    self.skip_char()
                return True
            elif self.prch(2):
                self.skip_char(3)
                return True
        elif self.s.startswith('\\*'):
            if self.str_at(2) == '(' and self.prch(3) and self.prch(4):
                reg = self.s[3:5]
                self.skip_char(5)
            elif self.str_at(2) == '[' and self.prch(3):
                self.skip_char(3)
                while self.str_at(0) and self.str_at(0) != ']':
                    reg = reg + self.str_at(0)
                    self.skip_char()
                if self.s.startswith(']'):
                    self.skip_char()
                else:
                    return False
            elif self.prch(2):
                reg = self.str_at(2)
                self.skip_char(3)
            else:
                return False
            
            if reg in self.reg_table:
                old_s = self.s
                self.s = self.reg_table[reg]
                self.text_arg()
                return True
        return False

    def size(self):
        if self.str_at(0) == '\\' and self.str_at(1) == 's' and (self.digit(2) or (self.str_at(2) in '-+' and self.digit(3))):
            self.skip_char(3)
            while self.digit(0): self.skip_char()
            return True
        return False

    def spec(self):
        self.specletter = False
        if self.s.startswith('\\(') and self.prch(2) and self.prch(3):
            specs_specletter = {
                # Output composed latin1 letters
                '-D': '\320',
                'Sd': '\360',
                'Tp': '\376',
                'TP': '\336',
                'AE': '\306',
                'ae': '\346',
                'OE': "OE",
                'oe': "oe",
                ':a': '\344',
                ':A': '\304',
                ':e': '\353',
                ':E': '\313',
                ':i': '\357',
                ':I': '\317',
                ':o': '\366',
                ':O': '\326',
                ':u': '\374',
                ':U': '\334',
                ':y': '\377',
                'ss': '\337',
                '\'A': '\301',
                '\'E': '\311',
                '\'I': '\315',
                '\'O': '\323',
                '\'U': '\332',
                '\'Y': '\335',
                '\'a': '\341',
                '\'e': '\351',
                '\'i': '\355',
                '\'o': '\363',
                '\'u': '\372',
                '\'y': '\375',
                '^A': '\302',
                '^E': '\312',
                '^I': '\316',
                '^O': '\324',
                '^U': '\333',
                '^a': '\342',
                '^e': '\352',
                '^i': '\356',
                '^o': '\364',
                '^u': '\373',
                '`A': '\300',
                '`E': '\310',
                '`I': '\314',
                '`O': '\322',
                '`U': '\331',
                '`a': '\340',
                '`e': '\350',
                '`i': '\354',
                '`o': '\362',
                '`u': '\371',
                '~A': '\303',
                '~N': '\321',
                '~O': '\325',
                '~a': '\343',
                '~n': '\361',
                '~o': '\365',
                ',C': '\307',
                ',c': '\347',
                '/l': "/l",
                '/L': "/L",
                '/o': '\370',
                '/O': '\330',
                'oA': '\305',
                'oa': '\345',
                
                # Ligatures
                'fi': 'fi',
                'ff': 'ff',
                'fl': 'fl',
                
                'Fi': 'ffi',
                'Ff': 'fff',
                'Fl': 'ffl'
            }
            
            specs = {
                'mi': '-',
                'en': '-',
                'hy': '-',
                'em': "--",
                'lq': "``",
                'rq': "\'\'",
                'Bq': ",,",
                'oq': '`',
                'cq': '\'',
                'aq': '\'',
                'dq': '"',
                'or': '|',
                'at': '@',
                'sh': '#',
                'Eu': '\244',
                'eu': '\244',
                'Do': '$',
                'ct': '\242',
                'Fo': '\253',
                'Fc': '\273',
                'fo': '<',
                'fc': '>',
                'r!': '\241',
                'r?': '\277',
                'Of': '\252',
                'Om': '\272',
                'pc': '\267',
                'S1': '\271',
                'S2': '\262',
                'S3': '\263',
                '<-': "<-",
                '->': "->",
                '<>': "<->",
                'ua': '^',
                'da': 'v',
                'lA': "<=",
                'rA': "=>",
                'hA': "<=>",
                'uA': "^^",
                'dA': "vv",
                'ba': '|',
                'bb': '|',
                'br': '|',
                'bv': '|',
                'ru': '_',
                'ul': '_',
                'ci': 'O',
                'bu': 'o',
                'co': '\251',
                'rg': '\256',
                'tm': "(TM)",
                'dd': "||",
                'dg': '|',
                'ps': '\266',
                'sc': '\247',
                'de': '\260',
                '%0': "0/00",
                '14': '\274',
                '12': '\275',
                '34': '\276',
                'f/': '/',
                'sl': '/',
                'rs': '\\',
                'sq': "[]",
                'fm': '\'',
                'ha': '^',
                'ti': '~',
                'lB': '[',
                'rB': ']',
                'lC': '{',
                'rC': '}',
                'la': '<',
                'ra': '>',
                'lh': "<=",
                'rh': "=>",
                'tf': "therefore",
                '~~': "~~",
                '~=': "~=",
                '!=': "!=",
                '**': '*',
                '+-': '\261',
                '<=': "<=",
                '==': "==",
                '=~': "=~",
                '>=': ">=",
                'AN': "\\/",
                'OR': "/\\",
                'no': '\254',
                'te': "there exists",
                'fa': "for all",
                'Ah': "aleph",
                'Im': "imaginary",
                'Re': "real",
                'if': "infinity",
                'md': "\267",
                'mo': "member of",
                'mu': '\327',
                'nm': "not member of",
                'pl': '+',
                'eq': '=',
                'pt': "oc",
                'pp': "perpendicular",
                'sb': "(=",
                'sp': "=)",
                'ib': "(-",
                'ip': "-)",
                'ap': '~',
                'is': 'I',
                'sr': "root",
                'pd': 'd',
                'c*': "(x)",
                'c+': "(+)",
                'ca': "cap",
                'cu': 'U',
                'di': '\367',
                'gr': 'V',
                'es': "{}",
                'CR': "_|",
                'st': "such that",
                '/_': "/_",
                'lz': "<>",
                'an': '-',
                
                # Output Greek
                '*A': "Alpha",
                '*B': "Beta",
                '*C': "Xi",
                '*D': "Delta",
                '*E': "Epsilon",
                '*F': "Phi",
                '*G': "Gamma",
                '*H': "Theta",
                '*I': "Iota",
                '*K': "Kappa",
                '*L': "Lambda",
                '*M': "Mu",
                '*N': "Nu",
                '*O': "Omicron",
                '*P': "Pi",
                '*Q': "Psi",
                '*R': "Rho",
                '*S': "Sigma",
                '*T': "Tau",
                '*U': "Upsilon",
                '*W': "Omega",
                '*X': "Chi",
                '*Y': "Eta",
                '*Z': "Zeta",
                '*a': "alpha",
                '*b': "beta",
                '*c': "xi",
                '*d': "delta",
                '*e': "epsilon",
                '*f': "phi",
                '+f': "phi",
                '*g': "gamma",
                '*h': "theta",
                '+h': "theta",
                '*i': "iota",
                '*k': "kappa",
                '*l': "lambda",
                '*m': "\265",
                '*n': "nu",
                '*o': "omicron",
                '*p': "pi",
                '+p': "omega",
                '*q': "psi",
                '*r': "rho",
                '*s': "sigma",
                '*t': "tau",
                '*u': "upsilon",
                '*w': "omega",
                '*x': "chi",
                '*y': "eta",
                '*z': "zeta",
                'ts': "sigma",
            }
            
            key = self.s[2:4]
            if key in specs_specletter:
                self.condputs(specs_specletter[key])
                self.specletter = True
            elif key in specs:
                self.condputs(specs[key])
            self.skip_char(4)
            return True
        elif self.s.startswith('\\%'):
            self.specletter = True
            self.skip_char(2)
            return True
        else:
            return False
            
    def esc(self):
        if self.str_at(0) == '\\' and self.str_at(1):
            c = self.str_at(1)
            if c in 'eE':
                self.condputchar('\\')
            elif c in 't':
                self.condputchar('\t')
            elif c in '0~':
                self.condputchar(' ')
            elif c in '|^&:':
                pass
            else:
                self.condputchar(c)
            self.skip_char(2)
            return True
        return False
    
    def word(self):
        if self.letter(0):
            self.condputchar(self.str_at(0))
            self.skip_char()
            while True:
                if self.spec() and not self.specletter:
                    break
                else:
                    if self.letter(0):
                        self.condputchar(self.str_at(0))
                        self.skip_char()
                    else:
                        break
            return True
        return False

    def text(self):
        while True:
            if not self.esc_char():
                if self.str_at(0):
                    self.condputchar(self.str_at(0))
                    self.skip_char()
                else:
                    break
        return True


    def letter(self, idx):
        ch = self.str_at(idx)
        return ch.isalpha() or ch == '_' # underscore is used in C identifiers

    
    def digit(self, idx):
        ch = self.str_at(idx)
        return ch.isdigit()

    def number(self):
        if (self.str_at(0) in '+-' and self.digit(1)) or self.digit(0):
            self.condputchar(self.str_at(0))
            self.skip_char()
            while self.digit(0):
                self.condputchar(self.str_at(0))
                self.skip_char()
            return True
        return False

    def esc_char(self):
        if self.s.startswith('\\'):
            if self.comment() or self.font() or self.size() or self.numreq() or self.var() or self.spec() or self.esc():
                return True
        return self.word() or self.number()

    def quoted_arg(self):
        if self.str_at(0) == '"':
            self.skip_char()
            while self.s and self.str_at(0) != '"':
                if not self.esc_char():
                    if self.s:
                        self.condputchar(self.str_at(0))
                        self.skip_char()
            return True
        else:
            return False
            
    def text_arg(self):
        if not self.esc_char():
            if self.s and not self.is_white(0):
                self.condputchar(self.str_at(0))
                self.skip_char()
            else:
                return False
        while True:
            if not self.esc_char():
                if self.s and not self.is_white(0):
                    self.condputchar(self.str_at(0))
                    self.skip_char()
                else:
                    return True

    def request_or_macro(self):
        self.skip_char()
        s0 = self.str_at(0)
        if s0 == '\\':
            if self.str_at(1) == '"':
                if not self.pretty: self.condputchar('\n')
                return True
            else:
                pass
        elif s0 == '[':
            self.refer = True
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0 == ']':
            self.refer = False
            self.skip_char()
            return self.text()
        elif s0 == '.':
            self.macro = False
            if not self.pretty: self.condputchar('\n')
            return True
        
        self.nobody = False
        s0s1 = self.s[0:2]
        if s0s1 == 'SH':
            for header_str in [' SYNOPSIS', ' "SYNOPSIS', ' ‹BERSICHT', ' "‹BERSICHT']:
                if self.s[2:].startswith(header_str):
                    if self.pretty: self.condputchar('\n')
                    self.inheader = True
                    break
            else:
                # Did not find a header string
                self.inheader = False
                if self.pretty: self.condputchar('\n')
                self.nobody = True
        elif s0s1 in ['SS', 'IP', 'H ']:
            if self.pretty: self.condputchar('\n')
            self.nobody = True
        elif s0s1 in ['I ', 'IR', 'IB', 'B ', 'BR', 'BI', 'R ', 'RB', 'RI', 'AB']:
            pass
        elif s0s1 in ['] ']:
            self.refer = False
        elif s0s1 in ['PS']:
            if self.is_white(2): self.pic = True
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['PE']:
            if self.is_white(2): self.pic = False
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['TS']:
            if self.is_white(2): self.tbl, self.tblstate = True, self.OPTIONS
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['T&']:
            if self.is_white(2): self.tbl, self.tblstate = True, self.FORMAT
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['TE']:
            if self.is_white(2): self.tbl = False
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['EQ']:
            if self.is_white(2): self.eqn = True
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['EN']:
            if self.is_white(2): self.eqn = False
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['R1']:
            if self.is_white(2): self.refer2 = True
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['R2']:
            if self.is_white(2): self.refer2 = False
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['de']:
            macro=True
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['BL', 'VL', 'AL', 'LB', 'RL', 'ML', 'DL']:
            if self.is_white(2): self.inlist = True
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['BV']:
            if self.str_at(2) == 'L' and self.white(self.str_at(3)): self.inlist = True
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['LE']:
            if self.is_white(2): self.inlist = False
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['LP', 'PP', 'P\n']:
            if self.pretty: self.condputchar('\n')
            self.condputchar('\n')
            return True
        elif s0s1 in ['ds']:
            self.skip_char(2)
            self.skip_leading_whitespace()
            if self.str_at(0):
                # Split at whitespace
                comps = self.s.split(None, 2)
                if len(comps) is 2:
                    name, value = comps
                    value = value.rstrip()
                    self.reg_table[name] = value
            if not self.pretty: self.condputchar('\n')
            return True
        elif s0s1 in ['so', 'nx']:
            # We always ignore include directives
            # deroff.c for some reason allowed this to fall through to the 'tr' case
            # I think that was just a bug so I won't replicate it
            return True
        elif s0s1 in ['tr']:
            self.skip_char(2)
            self.skip_leading_whitespace()
            while self.str_at(0) and self.str_at(0) != '\n':
                c = ord(self.str_at(0))
                self.skip_char()
                ns = self.str_at(0)
                self.skip_char()
                if not ns or ns == '\n':
                    self.tr[c] = ord(' ')
                else:
                    self.tr[c] = ord(ns)
            return True
        elif s0s1 in ['sp']:
            if self.pretty: self.condputchar('\n')
            self.condputchar('\n')
            return True
        else:
            if not self.pretty: self.condputchar('\n')
            return True
        
        if self.skipheaders and self.nobody: return True
        
        self.skip_leading_whitespace()
        while self.s and not self.is_white(0): self.skip_char()
        self.skip_leading_whitespace()
        while True:
            if not self.quoted_arg() and not self.text_arg():
                if self.s:
                    self.condputchar(self.str_at(0))
                    self.skip_char()
                else:
                    return True


    def do_tbl(self):
        if self.tblstate == self.OPTIONS:
            while self.s and self.str_at(0) != ';' and self.str_at(0) != '\n':
                self.skip_leading_whitespace()
                if not self.str_at(0).isalpha():
                    # deroff.c has a bug where it can loop forever here...we try to work around it
                    self.skip_char()
                else: # Parse option
                
                    option = self.s
                    arg = ''
                    
                    idx = 0
                    while option[idx:idx+1].isalpha():
                        idx += 1
                        
                    if option[idx:idx+1] == '(':
                        option = option[:idx]
                        self.s = self.s[idx+1:]
                        arg = self.s
                    else:
                        self.s = ''
                    
                    if arg:
                        idx = arg.find(')')
                        if idx != -1:
                            arg = arg[:idx]
                        self.s = self.s[idx+1:]
                    else:
                        #self.skip_char()
                        pass
                    
                    if option.lower() == 'tab':
                        self.tblTab = arg[0:1]
                        
            self.tblstate = self.FORMAT
            if not self.pretty: self.condputchar('\n')
            
        elif self.tblstate == self.FORMAT:
            while self.s and self.str_at(0) != '.' and self.str_at(0) != '\n':
                self.skip_leading_whitespace()
                if self.str_at(0): self.skip_char()
                
            if self.str_at(0) == '.': self.tblstate = self.DATA
            if not self.pretty: self.condputchar('\n')
        elif self.tblstate == self.DATA:
            if self.tblTab:
                self.s = self.s.replace(self.tblTab, '\t')
            self.text()
        return True

    def do_line(self):
        if not self.s: return True
        if self.str_at(0) == '.' or self.str_at(0) == '\'':
            if not self.request_or_macro(): return False
        elif self.tbl:
            self.do_tbl()
        else:
            self.text()
        return True
        
    def deroff(self, str):
        lines = str.split('\n')
        for line in lines:
            line = line + '\n'
            self.s = line
            if not self.do_line():
                break

def deroff_files(files):
    for arg in files:
        print >> sys.stderr, arg
        if arg.endswith('.gz'):
            f = gzip.open(arg, 'r')
        else:
            f = open(arg, 'r')
        str = f.read()
        d = Deroffer()
        d.deroff(str)
        d.flush_output(sys.stdout)
        f.close()

    

if __name__ == "__main__":
    import gzip
    paths = sys.argv[1:]
    if True:
        deroff_files(paths)
    else:
        import cProfile, pstats
        cProfile.run('deroff_files(paths)', 'fooprof')
        p = pstats.Stats('fooprof')
        p.sort_stats('cumulative').print_stats(100)
