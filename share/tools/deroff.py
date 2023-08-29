# -*- coding: utf-8 -*-

""" Deroff.py, ported to Python from the venerable deroff.c """

import sys, re, string

IS_PY3 = sys.version_info[0] >= 3


class Deroffer:
    g_specs_specletter = {
        # Output composed latin1 letters
        "-D": "\320",
        "Sd": "\360",
        "Tp": "\376",
        "TP": "\336",
        "AE": "\306",
        "ae": "\346",
        "OE": "OE",
        "oe": "oe",
        ":a": "\344",
        ":A": "\304",
        ":e": "\353",
        ":E": "\313",
        ":i": "\357",
        ":I": "\317",
        ":o": "\366",
        ":O": "\326",
        ":u": "\374",
        ":U": "\334",
        ":y": "\377",
        "ss": "\337",
        "'A": "\301",
        "'E": "\311",
        "'I": "\315",
        "'O": "\323",
        "'U": "\332",
        "'Y": "\335",
        "'a": "\341",
        "'e": "\351",
        "'i": "\355",
        "'o": "\363",
        "'u": "\372",
        "'y": "\375",
        "^A": "\302",
        "^E": "\312",
        "^I": "\316",
        "^O": "\324",
        "^U": "\333",
        "^a": "\342",
        "^e": "\352",
        "^i": "\356",
        "^o": "\364",
        "^u": "\373",
        "`A": "\300",
        "`E": "\310",
        "`I": "\314",
        "`O": "\322",
        "`U": "\331",
        "`a": "\340",
        "`e": "\350",
        "`i": "\354",
        "`o": "\362",
        "`u": "\371",
        "~A": "\303",
        "~N": "\321",
        "~O": "\325",
        "~a": "\343",
        "~n": "\361",
        "~o": "\365",
        ",C": "\307",
        ",c": "\347",
        "/l": "/l",
        "/L": "/L",
        "/o": "\370",
        "/O": "\330",
        "oA": "\305",
        "oa": "\345",
        # Ligatures
        "fi": "fi",
        "ff": "ff",
        "fl": "fl",
        "Fi": "ffi",
        "Ff": "fff",
        "Fl": "ffl",
    }

    g_specs = {
        "mi": "-",
        "en": "-",
        "hy": "-",
        "em": "--",
        "lq": "“",
        "rq": "”",
        "Bq": ",,",
        "oq": "`",
        "cq": "'",
        "aq": "'",
        "dq": '"',
        "or": "|",
        "at": "@",
        "sh": "#",
        "Eu": "\244",
        "eu": "\244",
        "Do": "$",
        "ct": "\242",
        "Fo": "\253",
        "Fc": "\273",
        "fo": "<",
        "fc": ">",
        "r!": "\241",
        "r?": "\277",
        "Of": "\252",
        "Om": "\272",
        "pc": "\267",
        "S1": "\271",
        "S2": "\262",
        "S3": "\263",
        "<-": "<-",
        "->": "->",
        "<>": "<->",
        "ua": "^",
        "da": "v",
        "lA": "<=",
        "rA": "=>",
        "hA": "<=>",
        "uA": "^^",
        "dA": "vv",
        "ba": "|",
        "bb": "|",
        "br": "|",
        "bv": "|",
        "ru": "_",
        "ul": "_",
        "ci": "O",
        "bu": "o",
        "co": "\251",
        "rg": "\256",
        "tm": "(TM)",
        "dd": "||",
        "dg": "|",
        "ps": "\266",
        "sc": "\247",
        "de": "\260",
        "%0": "0/00",
        "14": "\274",
        "12": "\275",
        "34": "\276",
        "f/": "/",
        "sl": "/",
        "rs": "\\",
        "sq": "[]",
        "fm": "'",
        "ha": "^",
        "ti": "~",
        "lB": "[",
        "rB": "]",
        "lC": "{",
        "rC": "}",
        "la": "<",
        "ra": ">",
        "lh": "<=",
        "rh": "=>",
        "tf": "therefore",
        "~~": "~~",
        "~=": "~=",
        "!=": "!=",
        "**": "*",
        "+-": "\261",
        "<=": "<=",
        "==": "==",
        "=~": "=~",
        ">=": ">=",
        "AN": "\\/",
        "OR": "/\\",
        "no": "\254",
        "te": "there exists",
        "fa": "for all",
        "Ah": "aleph",
        "Im": "imaginary",
        "Re": "real",
        "if": "infinity",
        "md": "\267",
        "mo": "member of",
        "mu": "\327",
        "nm": "not member of",
        "pl": "+",
        "eq": "=",
        "pt": "oc",
        "pp": "perpendicular",
        "sb": "(=",
        "sp": "=)",
        "ib": "(-",
        "ip": "-)",
        "ap": "~",
        "is": "I",
        "sr": "root",
        "pd": "d",
        "c*": "(x)",
        "c+": "(+)",
        "ca": "cap",
        "cu": "U",
        "di": "\367",
        "gr": "V",
        "es": "{}",
        "CR": "_|",
        "st": "such that",
        "/_": "/_",
        "lz": "<>",
        "an": "-",
        # Output Greek
        "*A": "Alpha",
        "*B": "Beta",
        "*C": "Xi",
        "*D": "Delta",
        "*E": "Epsilon",
        "*F": "Phi",
        "*G": "Gamma",
        "*H": "Theta",
        "*I": "Iota",
        "*K": "Kappa",
        "*L": "Lambda",
        "*M": "Mu",
        "*N": "Nu",
        "*O": "Omicron",
        "*P": "Pi",
        "*Q": "Psi",
        "*R": "Rho",
        "*S": "Sigma",
        "*T": "Tau",
        "*U": "Upsilon",
        "*W": "Omega",
        "*X": "Chi",
        "*Y": "Eta",
        "*Z": "Zeta",
        "*a": "alpha",
        "*b": "beta",
        "*c": "xi",
        "*d": "delta",
        "*e": "epsilon",
        "*f": "phi",
        "+f": "phi",
        "*g": "gamma",
        "*h": "theta",
        "+h": "theta",
        "*i": "iota",
        "*k": "kappa",
        "*l": "lambda",
        "*m": "\265",
        "*n": "nu",
        "*o": "omicron",
        "*p": "pi",
        "+p": "omega",
        "*q": "psi",
        "*r": "rho",
        "*s": "sigma",
        "*t": "tau",
        "*u": "upsilon",
        "*w": "omega",
        "*x": "chi",
        "*y": "eta",
        "*z": "zeta",
        "ts": "sigma",
    }

    g_re_word = re.compile(r"[a-zA-Z_]+")  # equivalent to the word() method
    g_re_number = re.compile(r"[+-]?\d+")  # equivalent to the number() method
    g_re_esc_char = re.compile(
        r"""([a-zA-Z_]) |   # Word
                                   ([+-]?\d)   |   # Number
                                   \\              # Backslash (for escape seq)
                               """,
        re.VERBOSE,
    )

    g_re_not_backslash_or_whitespace = re.compile(
        r"[^ \t\n\r\f\v\\]+"
    )  # Match a sequence of not backslash or whitespace

    g_re_newline_collapse = re.compile(r"\n{3,}")

    g_re_font = re.compile(
        r"""\\f(         # Starts with backslash f
                               (\(\S{2}) |  # Open paren, then two printable chars
                               (\[\S*?\]) |  # Open bracket, zero or more printable characters, then close bracket
                               \S)          # Any printable character
                            """,
        re.VERBOSE,
    )

    # This gets filled in in __init__ below
    g_macro_dict = False

    def __init__(self):
        self.reg_table = {}
        self.tr_from = ""
        self.tr_to = ""
        self.tr = ""
        self.nls = 2
        self.specletter = False
        self.refer = False
        self.macro = 0
        self.nobody = False
        self.inlist = False
        self.inheader = False
        self.pic = False
        self.tbl = False
        self.tblstate = 0
        self.tblTab = ""
        self.eqn = False
        self.skipheaders = False
        self.skiplists = False
        self.ignore_sonx = False
        self.output = []
        self.name = ""

        self.OPTIONS = 0
        self.FORMAT = 1
        self.DATA = 2

        # words is uninteresting and should be treated as false

        if not Deroffer.g_macro_dict:
            Deroffer.g_macro_dict = {
                "SH": Deroffer.macro_sh,
                "SS": Deroffer.macro_ss_ip,
                "IP": Deroffer.macro_ss_ip,
                "H ": Deroffer.macro_ss_ip,
                "I ": Deroffer.macro_i_ir,
                "IR": Deroffer.macro_i_ir,
                "IB": Deroffer.macro_i_ir,
                "B ": Deroffer.macro_i_ir,
                "BR": Deroffer.macro_i_ir,
                "BI": Deroffer.macro_i_ir,
                "R ": Deroffer.macro_i_ir,
                "RB": Deroffer.macro_i_ir,
                "RI": Deroffer.macro_i_ir,
                "AB": Deroffer.macro_i_ir,
                "Nm": Deroffer.macro_Nm,
                "] ": Deroffer.macro_close_bracket,
                "PS": Deroffer.macro_ps,
                "PE": Deroffer.macro_pe,
                "TS": Deroffer.macro_ts,
                "T&": Deroffer.macro_t_and,
                "TE": Deroffer.macro_te,
                "EQ": Deroffer.macro_eq,
                "EN": Deroffer.macro_en,
                "R1": Deroffer.macro_r1,
                "R2": Deroffer.macro_r2,
                "de": Deroffer.macro_de,
                "BL": Deroffer.macro_bl_vl,
                "VL": Deroffer.macro_bl_vl,
                "AL": Deroffer.macro_bl_vl,
                "LB": Deroffer.macro_bl_vl,
                "RL": Deroffer.macro_bl_vl,
                "ML": Deroffer.macro_bl_vl,
                "DL": Deroffer.macro_bl_vl,
                "BV": Deroffer.macro_bv,
                "LE": Deroffer.macro_le,
                "LP": Deroffer.macro_lp_pp,
                "PP": Deroffer.macro_lp_pp,
                "P\n": Deroffer.macro_lp_pp,
                "ds": Deroffer.macro_ds,
                "so": Deroffer.macro_so_nx,
                "nx": Deroffer.macro_so_nx,
                "tr": Deroffer.macro_tr,
                "sp": Deroffer.macro_sp,
            }

    def flush_output(self, where):
        if where:
            where.write(self.get_output())
        self.output[:] = []

    def get_output(self):
        res = "".join(self.output)
        clean_res = Deroffer.g_re_newline_collapse.sub("\n", res)
        return clean_res

    def putchar(self, c):
        self.output.append(c)
        return c

    # This gets swapped in in place of condputs the first time tr gets modified
    def condputs_tr(self, str):
        special = (
            self.pic
            or self.eqn
            or self.refer
            or self.macro
            or (self.skiplists and self.inlist)
            or (self.skipheaders and self.inheader)
        )
        if not special:
            self.output.append(str.translate(self.tr))

    def condputs(self, str):
        special = (
            self.pic
            or self.eqn
            or self.refer
            or self.macro
            or (self.skiplists and self.inlist)
            or (self.skipheaders and self.inheader)
        )
        if not special:
            self.output.append(str)

    def str_at(self, idx):
        return self.s[idx : idx + 1]

    def skip_char(self, amt=1):
        self.s = self.s[amt:]

    def skip_leading_whitespace(self):
        self.s = self.s.lstrip()

    def is_white(self, idx):
        # Note this returns false for empty strings (idx >= len(self.s))
        return self.s[idx : idx + 1].isspace()

    def str_eq(offset, other, len):
        return self.s[offset : offset + len] == other[:len]

    def prch(self, idx):
        # Note that this return False for the empty string (idx >= len(self.s))
        ch = self.s[idx : idx + 1]
        return ch not in " \t\n"

    def font(self):
        match = Deroffer.g_re_font.match(self.s)
        if not match:
            return False
        self.skip_char(match.end())
        return True

    def font2(self):
        if self.s[0:2] == "\\f":
            c = self.str_at(2)
            if c == "(" and self.prch(3) and self.prch(4):
                self.skip_char(5)
                return True
            elif c == "[":
                self.skip_char(2)
                while self.prch(0) and self.str_at(0) != "]":
                    self.skip_char()
                if self.str_at(0) == "]":
                    self.skip_char()
            elif self.prch(2):
                self.skip_char(3)
                return True
        return False

    def comment(self):
        # Here we require that the string start with \"
        while self.str_at(0) and self.str_at(0) != "\n":
            self.skip_char()
        return True

    def numreq(self):
        # We require that the string starts with backslash
        if self.str_at(1) in "hvwud" and self.str_at(2) == "'":
            self.macro += 1
            self.skip_char(3)
            while self.str_at(0) != "'" and self.esc_char():
                pass  # Weird
            if self.str_at(0) == "'":
                self.skip_char()
            self.macro -= 1
            return True
        return False

    def var(self):
        reg = ""
        s0s1 = self.s[0:2]
        if s0s1 == "\\n":
            if self.s[3:5] == "dy":
                self.skip_char(5)
                return True
            elif self.str_at(2) == "(" and self.prch(3) and self.prch(4):
                self.skip_char(5)
                return True
            elif self.str_at(2) == "[" and self.prch(3):
                self.skip_char(3)
                while self.str_at(0) and self.str_at(0) != "]":
                    self.skip_char()
                return True
            elif self.prch(2):
                self.skip_char(3)
                return True
        elif s0s1 == "\\*":
            if self.str_at(2) == "(" and self.prch(3) and self.prch(4):
                reg = self.s[3:5]
                self.skip_char(5)
            elif self.str_at(2) == "[" and self.prch(3):
                self.skip_char(3)
                while self.str_at(0) and self.str_at(0) != "]":
                    reg = reg + self.str_at(0)
                    self.skip_char()
                if self.s[0:1] == "]":
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
        # We require that the string starts with \s
        if self.digit(2) or (self.str_at(2) in "-+" and self.digit(3)):
            self.skip_char(3)
            while self.digit(0):
                self.skip_char()
            return True
        return False

    def spec(self):
        self.specletter = False
        if self.s[0:2] == "\\(" and self.prch(2) and self.prch(3):
            key = self.s[2:4]
            if key in Deroffer.g_specs_specletter:
                self.condputs(Deroffer.g_specs_specletter[key])
                self.specletter = True
            elif key in Deroffer.g_specs:
                self.condputs(Deroffer.g_specs[key])
            self.skip_char(4)
            return True
        elif self.s.startswith("\\%"):
            self.specletter = True
            self.skip_char(2)
            return True
        else:
            return False

    def esc(self):
        # We require that the string start with backslash
        c = self.s[1:2]
        if not c:
            return False
        if c in "eE":
            self.condputs("\\")
        elif c in "t":
            self.condputs("\t")
        elif c in "0~":
            self.condputs(" ")
        elif c in "|^&:":
            pass
        else:
            self.condputs(c)
        self.skip_char(2)
        return True

    def word(self):
        got_something = False
        while True:
            match = Deroffer.g_re_word.match(self.s)
            if not match:
                break
            got_something = True
            self.condputs(match.group(0))
            self.skip_char(match.end(0))

            # Consume all specials
            while self.spec():
                if not self.specletter:
                    break

        return got_something

    def text(self):
        while True:
            idx = self.s.find("\\")
            if idx == -1:
                self.condputs(self.s)
                self.s = ""
                break
            else:
                self.condputs(self.s[:idx])
                self.skip_char(idx)
                if not self.esc_char_backslash():
                    self.condputs(self.str_at(0))
                    self.skip_char()
        return True

    def letter(self, idx):
        ch = self.str_at(idx)
        return ch.isalpha() or ch == "_"  # underscore is used in C identifiers

    def digit(self, idx):
        ch = self.str_at(idx)
        return ch.isdigit()

    def number(self):
        match = Deroffer.g_re_number.match(self.s)
        if not match:
            return False
        else:
            self.condputs(match.group(0))
            self.skip_char(match.end())
            return True

    def esc_char_backslash(self):
        # Like esc_char, but we know the string starts with a backslash
        c = self.s[1:2]
        if c == '"':
            return self.comment()
        elif c == "f":
            return self.font()
        elif c == "s":
            return self.size()
        elif c in "hvwud":
            return self.numreq()
        elif c in "n*":
            return self.var()
        elif c == "(":
            return self.spec()
        else:
            return self.esc()

    def esc_char(self):
        if self.s[0:1] == "\\":
            return self.esc_char_backslash()
        return self.word() or self.number()

    def quoted_arg(self):
        if self.str_at(0) == '"':
            self.skip_char()
            while self.s and self.str_at(0) != '"':
                if not self.esc_char():
                    if self.s:
                        self.condputs(self.str_at(0))
                        self.skip_char()
            return True
        else:
            return False

    def text_arg(self):
        # PCA: The deroff.c textArg() disallowed quotes at the start of an argument
        # I'm not sure if this was a bug or not
        got_something = False
        while True:
            match = Deroffer.g_re_not_backslash_or_whitespace.match(self.s)
            if match:
                # Output the characters in the match
                self.condputs(match.group(0))
                self.skip_char(match.end(0))
                got_something = True

            # Next is either an escape, or whitespace, or the end
            # If it's the whitespace or the end, we're done
            if not self.s or self.is_white(0):
                return got_something

            # Try an escape
            if not self.esc_char():
                # Some busted escape? Just output it
                self.condputs(self.str_at(0))
                self.skip_char()
                got_something = True

    def text_arg2(self):
        if not self.esc_char():
            if self.s and not self.is_white(0):
                self.condputs(self.str_at(0))
                self.skip_char()
            else:
                return False
        while True:
            if not self.esc_char():
                if self.s and not self.is_white(0):
                    self.condputs(self.str_at(0))
                    self.skip_char()
                else:
                    return True

    # Macro functions
    def macro_sh(self):
        for header_str in [" SYNOPSIS", ' "SYNOPSIS', " ‹BERSICHT", ' "‹BERSICHT']:
            if self.s[2:].startswith(header_str):
                self.inheader = True
                break
        else:
            # Did not find a header string
            self.inheader = False
            self.nobody = True

    def macro_ss_ip(self):
        self.nobody = True
        return False

    def macro_i_ir(self):
        return False

    def macro_Nm(self):
        if self.s == "Nm\n":
            self.condputs(self.name)
        else:
            self.name = self.s[3:].strip() + " "
        return True

    def macro_close_bracket(self):
        self.refer = False
        return False

    def macro_ps(self):
        if self.is_white(2):
            self.pic = True
        self.condputs("\n")
        return True

    def macro_pe(self):
        if self.is_white(2):
            self.pic = False
        self.condputs("\n")
        return True

    def macro_ts(self):
        if self.is_white(2):
            self.tbl, self.tblstate = True, self.OPTIONS
        self.condputs("\n")
        return True

    def macro_t_and(self):
        if self.is_white(2):
            self.tbl, self.tblstate = True, self.FORMAT
        self.condputs("\n")
        return True

    def macro_te(self):
        if self.is_white(2):
            self.tbl = False
        self.condputs("\n")
        return True

    def macro_eq(self):
        if self.is_white(2):
            self.eqn = True
        self.condputs("\n")
        return True

    def macro_en(self):
        if self.is_white(2):
            self.eqn = False
        self.condputs("\n")
        return True

    def macro_r1(self):
        if self.is_white(2):
            self.refer2 = True
        self.condputs("\n")
        return True

    def macro_r2(self):
        if self.is_white(2):
            self.refer2 = False
        self.condputs("\n")
        return True

    def macro_de(self):
        macro = True
        self.condputs("\n")
        return True

    def macro_bl_vl(self):
        if self.is_white(2):
            self.inlist = True
        self.condputs("\n")
        return True

    def macro_bv(self):
        if self.str_at(2) == "L" and self.white(self.str_at(3)):
            self.inlist = True
        self.condputs("\n")
        return True

    def macro_le(self):
        if self.is_white(2):
            self.inlist = False
        self.condputs("\n")
        return True

    def macro_lp_pp(self):
        self.condputs("\n")
        return True

    def macro_ds(self):
        self.skip_char(2)
        self.skip_leading_whitespace()
        if self.str_at(0):
            # Split at whitespace
            comps = self.s.split(None, 2)
            if len(comps) == 2:
                name, value = comps
                value = value.rstrip()
                self.reg_table[name] = value
        self.condputs("\n")
        return True

    def macro_so_nx(self):
        # We always ignore include directives
        # deroff.c for some reason allowed this to fall through to the 'tr' case
        # I think that was just a bug so I won't replicate it
        return True

    def macro_tr(self):
        self.skip_char(2)
        self.skip_leading_whitespace()
        while self.s and self.str_at(0) != "\n":
            c = self.str_at(0)
            ns = self.str_at(1)
            self.skip_char(2)
            if not ns or ns == "\n":
                ns = " "
            self.tr_from += c
            self.tr_to += ns

        # Update our table, then swap in the slower tr-savvy condputs
        try:  # Python2
            self.tr = string.maketrans(self.tr_from, self.tr_to)
        except AttributeError:  # Python3
            self.tr = "".maketrans(self.tr_from, self.tr_to)
        self.condputs = self.condputs_tr
        return True

    def macro_sp(self):
        self.condputs("\n")
        return True

    def macro_other(self):
        self.condputs("\n")
        return True

    def request_or_macro(self):
        # s[0] is period or open single quote
        self.skip_char()
        s0 = self.s[1:2]
        if s0 == "\\":
            if self.str_at(1) == '"':
                self.condputs("\n")
                return True
            else:
                pass
        elif s0 == "[":
            self.refer = True
            self.condputs("\n")
            return True
        elif s0 == "]":
            self.refer = False
            self.skip_char()
            return self.text()
        elif s0 == ".":
            self.macro = False
            self.condputs("\n")
            return True

        self.nobody = False
        s0s1 = self.s[0:2]

        macro_func = Deroffer.g_macro_dict.get(s0s1, Deroffer.macro_other)
        if macro_func(self):
            return True

        if self.skipheaders and self.nobody:
            return True

        self.skip_leading_whitespace()
        while self.s and not self.is_white(0):
            self.skip_char()
        self.skip_leading_whitespace()
        while True:
            if not self.quoted_arg() and not self.text_arg():
                if self.s:
                    self.condputs(self.str_at(0))
                    self.skip_char()
                else:
                    return True

    def request_or_macro2(self):
        self.skip_char()
        s0 = self.s[0:1]
        if s0 == "\\":
            if self.str_at(1) == '"':
                self.condputs("\n")
                return True
            else:
                pass
        elif s0 == "[":
            self.refer = True
            self.condputs("\n")
            return True
        elif s0 == "]":
            self.refer = False
            self.skip_char()
            return self.text()
        elif s0 == ".":
            self.macro = False
            self.condputs("\n")
            return True

        self.nobody = False
        s0s1 = self.s[0:2]
        if s0s1 == "SH":
            for header_str in [" SYNOPSIS", ' "SYNOPSIS', " ‹BERSICHT", ' "‹BERSICHT']:
                if self.s[2:].startswith(header_str):
                    self.inheader = True
                    break
            else:
                # Did not find a header string
                self.inheader = False
                self.nobody = True
        elif s0s1 in ["SS", "IP", "H "]:
            self.nobody = True
        elif s0s1 in ["I ", "IR", "IB", "B ", "BR", "BI", "R ", "RB", "RI", "AB"]:
            pass
        elif s0s1 in ["] "]:
            self.refer = False
        elif s0s1 in ["PS"]:
            if self.is_white(2):
                self.pic = True
            self.condputs("\n")
            return True
        elif s0s1 in ["PE"]:
            if self.is_white(2):
                self.pic = False
            self.condputs("\n")
            return True
        elif s0s1 in ["TS"]:
            if self.is_white(2):
                self.tbl, self.tblstate = True, self.OPTIONS
            self.condputs("\n")
            return True
        elif s0s1 in ["T&"]:
            if self.is_white(2):
                self.tbl, self.tblstate = True, self.FORMAT
            self.condputs("\n")
            return True
        elif s0s1 in ["TE"]:
            if self.is_white(2):
                self.tbl = False
            self.condputs("\n")
            return True
        elif s0s1 in ["EQ"]:
            if self.is_white(2):
                self.eqn = True
            self.condputs("\n")
            return True
        elif s0s1 in ["EN"]:
            if self.is_white(2):
                self.eqn = False
            self.condputs("\n")
            return True
        elif s0s1 in ["R1"]:
            if self.is_white(2):
                self.refer2 = True
            self.condputs("\n")
            return True
        elif s0s1 in ["R2"]:
            if self.is_white(2):
                self.refer2 = False
            self.condputs("\n")
            return True
        elif s0s1 in ["de"]:
            macro = True
            self.condputs("\n")
            return True
        elif s0s1 in ["BL", "VL", "AL", "LB", "RL", "ML", "DL"]:
            if self.is_white(2):
                self.inlist = True
            self.condputs("\n")
            return True
        elif s0s1 in ["BV"]:
            if self.str_at(2) == "L" and self.white(self.str_at(3)):
                self.inlist = True
            self.condputs("\n")
            return True
        elif s0s1 in ["LE"]:
            if self.is_white(2):
                self.inlist = False
            self.condputs("\n")
            return True
        elif s0s1 in ["LP", "PP", "P\n"]:
            self.condputs("\n")
            return True
        elif s0s1 in ["ds"]:
            self.skip_char(2)
            self.skip_leading_whitespace()
            if self.str_at(0):
                # Split at whitespace
                comps = self.s.split(None, 2)
                if len(comps) == 2:
                    name, value = comps
                    value = value.rstrip()
                    self.reg_table[name] = value
            self.condputs("\n")
            return True
        elif s0s1 in ["so", "nx"]:
            # We always ignore include directives
            # deroff.c for some reason allowed this to fall through to the 'tr' case
            # I think that was just a bug so I won't replicate it
            return True
        elif s0s1 in ["tr"]:
            self.skip_char(2)
            self.skip_leading_whitespace()
            while self.s and self.str_at(0) != "\n":
                c = self.str_at(0)
                ns = self.str_at(1)
                self.skip_char(2)
                if not ns or ns == "\n":
                    ns = " "
                self.tr_from += c
                self.tr_to += ns

            # Update our table, then swap in the slower tr-savvy condputs
            try:  # Python2
                self.tr = string.maketrans(self.tr_from, self.tr_to)
            except AttributeError:  # Python3
                self.tr = "".maketrans(self.tr_from, self.tr_to)
            self.condputs = self.condputs_tr

            return True
        elif s0s1 in ["sp"]:
            self.condputs("\n")
            return True
        else:
            self.condputs("\n")
            return True

        if self.skipheaders and self.nobody:
            return True

        self.skip_leading_whitespace()
        while self.s and not self.is_white(0):
            self.skip_char()
        self.skip_leading_whitespace()
        while True:
            if not self.quoted_arg() and not self.text_arg():
                if self.s:
                    self.condputs(self.str_at(0))
                    self.skip_char()
                else:
                    return True

    def do_tbl(self):
        if self.tblstate == self.OPTIONS:
            while self.s and self.str_at(0) != ";" and self.str_at(0) != "\n":
                self.skip_leading_whitespace()
                if not self.str_at(0).isalpha():
                    # deroff.c has a bug where it can loop forever here...we try to work around it
                    self.skip_char()
                else:  # Parse option
                    option = self.s
                    arg = ""

                    idx = 0
                    while option[idx : idx + 1].isalpha():
                        idx += 1

                    if option[idx : idx + 1] == "(":
                        option = option[:idx]
                        self.s = self.s[idx + 1 :]
                        arg = self.s
                    else:
                        self.s = ""

                    if arg:
                        idx = arg.find(")")
                        if idx != -1:
                            arg = arg[:idx]
                        self.s = self.s[idx + 1 :]
                    else:
                        # self.skip_char()
                        pass

                    if option.lower() == "tab":
                        self.tblTab = arg[0:1]

            self.tblstate = self.FORMAT
            self.condputs("\n")

        elif self.tblstate == self.FORMAT:
            while self.s and self.str_at(0) != "." and self.str_at(0) != "\n":
                self.skip_leading_whitespace()
                if self.str_at(0):
                    self.skip_char()

            if self.str_at(0) == ".":
                self.tblstate = self.DATA
            self.condputs("\n")
        elif self.tblstate == self.DATA:
            if self.tblTab:
                self.s = self.s.replace(self.tblTab, "\t")
            self.text()
        return True

    def do_line(self):
        if self.s[0:1] in ".'":
            if not self.request_or_macro():
                return False
        elif self.tbl:
            self.do_tbl()
        else:
            self.text()
        return True

    def deroff(self, str):
        lines = str.split("\n")
        for line in lines:
            self.s = line + "\n"
            if not self.do_line():
                break
            # self.putchar('\n')


def deroff_files(files):
    for arg in files:
        sys.stderr.write(arg + "\n")
        if arg.endswith(".gz"):
            f = gzip.open(arg, "r")
            str = f.read()
            if IS_PY3:
                str = str.decode("latin-1")
        else:
            f = open(arg, "r")
            str = f.read()
        d = Deroffer()
        d.deroff(str)
        d.flush_output(sys.stdout)
        f.close()


if __name__ == "__main__":
    import gzip

    paths = sys.argv[1:]
    deroff_files(paths)
    #    import cProfile, profile, pstats
    #    profile.run("deroff_files(paths)", "fooprof")
    #    p = pstats.Stats("fooprof")
    #    p.sort_stats("time").print_stats(100)
    #    p.sort_stats('calls').print_callers(.5, 'startswith')
