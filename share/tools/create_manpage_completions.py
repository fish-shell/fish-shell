#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# Run me like this: ./create_manpage_completions.py /usr/share/man/man{1,8}/* > man_completions.fish

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

import argparse
import bz2
import errno
import gzip
import os
import re
import string
import subprocess
import sys
import traceback
from subprocess import DEVNULL

lzma_available = True
try:
    import lzma
except ImportError:
    lzma_available = False


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
        # novermin
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
        # novermin
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

    def str_eq(self, offset, other, len):
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
        self.tr = str.maketrans(self.tr_from, self.tr_to)
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
            self.tr = str.maketrans(self.tr_from, self.tr_to)
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


# This gets set to the name of the command that we are currently executing
CMDNAME = ""

# Information used to track which of our parsers were successful
PARSER_INFO = {}

# built_command writes into this global variable, yuck
built_command_output = []

# Diagnostic output
diagnostic_output = []
diagnostic_indent = 0

# Three diagnostic verbosity levels
VERY_VERBOSE, BRIEF_VERBOSE, NOT_VERBOSE = 2, 1, 0

# Pick some reasonable default values for settings
VERBOSITY, WRITE_TO_STDOUT, DEROFF_ONLY, KEEP_FILES = NOT_VERBOSE, False, False, False


def add_diagnostic(dgn, msg_verbosity=VERY_VERBOSE):
    # Add a diagnostic message, if msg_verbosity <= VERBOSITY
    if msg_verbosity <= VERBOSITY:
        diagnostic_output.append("   " * diagnostic_indent + dgn)


def flush_diagnostics(where):
    if diagnostic_output:
        output_str = "\n".join(diagnostic_output)
        print(output_str, file=where)
        diagnostic_output[:] = []


# Make sure we don't output the same completion multiple times, which can happen
# For example, xsubpp.1.gz and xsubpp5.10.1.gz
# This maps commands to lists of completions
already_output_completions = {}


def compile_and_search(regex, input):
    options_section_regex = re.compile(regex, re.DOTALL)
    options_section_matched = re.search(options_section_regex, input)
    return options_section_matched


def unquote_double_quotes(data):
    if len(data) < 2:
        return data
    if data[0] == '"' and data[len(data) - 1] == '"':
        data = data[1 : len(data) - 1]
    return data


def unquote_single_quotes(data):
    if len(data) < 2:
        return data
    if data[0] == "`" and data[len(data) - 1] == "'":
        data = data[1 : len(data) - 1]
    return data


# Make a string of characters that are deemed safe in fish without needing to be escaped
# Note that space is not included
g_fish_safe_chars = frozenset(string.ascii_letters + string.digits + "_+-|/:=@~")


def fish_escape_single_quote(str):
    # Escape a string if necessary so that it can be put in single quotes
    # If it has no non-safe chars, there's nothing to do
    if g_fish_safe_chars.issuperset(str):
        return str

    str = str.replace("\\", "\\\\")  # Replace one backslash with two
    str = str.replace(
        "'", "\\'"
    )  # Replace one single quote with a backslash-single-quote
    return "'" + str + "'"


def output_complete_command(cmdname, args, description, output_list):
    comps = ["complete -c", cmdname]
    comps.extend(args)
    if description:
        comps.append("-d")
        comps.append(description)
    output_list.append(" ".join(comps))


def built_command(options, description):
    #    print "Options are: ", options
    man_optionlist = re.split(' |,|"|=|[|]', options)
    fish_options = []
    for optionstr in man_optionlist:
        option = re.sub(r"(\[.*\])", "", optionstr)
        option = option.strip(" \t\r\n[](){}.,:!")

        # Skip some problematic cases
        if option in ["-", "--"]:
            continue
        if any(c in "{}()" for c in option):
            continue

        if option.startswith("--"):
            # New style long option (--recursive)
            fish_options.append("-l " + fish_escape_single_quote(option[2:]))
        elif option.startswith("-") and len(option) == 2:
            # New style short option (-r)
            fish_options.append("-s " + fish_escape_single_quote(option[1:]))
        elif option.startswith("-") and len(option) > 2:
            # Old style long option (-recursive)
            fish_options.append("-o " + fish_escape_single_quote(option[1:]))

    # Determine which options are new (not already in existing_options)
    # Then add those to the existing options
    existing_options = already_output_completions.setdefault(CMDNAME, set())
    fish_options = [opt for opt in fish_options if opt not in existing_options]
    existing_options.update(fish_options)

    # Maybe it's all for naught
    if not fish_options:
        return

    # Here's what we'll use to truncate if necessary
    max_description_width = 78
    truncation_suffix = "…"

    # Try to include as many whole sentences as will fit
    # Clean up some probably bogus escapes in the process
    clean_desc = description.replace("\\'", "'").replace("\\.", ".")
    sentences = clean_desc.split(".")

    # Clean up "sentences" that are just whitespace
    # But don't let it be empty
    sentences = [x for x in sentences if x.strip()]
    if not sentences:
        sentences = [""]

    udot = "."
    uspace = " "

    truncated_description = sentences[0] + udot
    for line in sentences[1:]:
        if not line:
            continue
        proposed_description = truncated_description + uspace + line + udot
        if len(proposed_description) <= max_description_width:
            # It fits
            truncated_description = proposed_description
        else:
            # No fit
            break
    # Strip trailing dots
    truncated_description = truncated_description.strip(udot)

    # If the first sentence does not fit, truncate if necessary
    if len(truncated_description) > max_description_width:
        prefix_len = max_description_width - len(truncation_suffix)
        truncated_description = truncated_description[:prefix_len] + truncation_suffix

    # Escape some more things
    truncated_description = fish_escape_single_quote(truncated_description)
    escaped_cmd = fish_escape_single_quote(CMDNAME)

    output_complete_command(
        escaped_cmd, fish_options, truncated_description, built_command_output
    )


def remove_groff_formatting(data):
    data = data.replace("\\fI", "")
    data = data.replace("\\fP", "")
    data = data.replace("\\f1", "")
    data = data.replace("\\fB", "")
    data = data.replace("\\fR", "")
    data = data.replace("\\e", "")
    data = re.sub(r".PD( \d+)", "", data)
    data = data.replace(".BI", "")
    data = data.replace(".BR", "")
    data = data.replace("0.5i", "")
    data = data.replace(".rb", "")
    data = data.replace("\\^", "")
    data = data.replace("{ ", "")
    data = data.replace(" }", "")
    data = data.replace(r"\ ", "")
    data = data.replace(r"\-", "-")
    data = data.replace(r"\&", "")
    data = data.replace(".B", "")
    data = data.replace(r"\-", "-")
    data = data.replace(".I", "")
    data = data.replace("\f", "")
    data = data.replace(r"\(oq", "'")
    data = data.replace(r"\(cq", "'")
    data = data.replace(r"\(aq", "'")
    data = data.replace(r"\(dq", '"')
    data = data.replace(r"\(lq", '"')
    data = data.replace(r"\(rq", '"')
    return data


class ManParser(object):
    def is_my_type(self, manpage):
        return False

    def parse_man_page(self, manpage):
        return False


class Type1ManParser(ManParser):
    def is_my_type(self, manpage):
        return compile_and_search(r'\.SH "OPTIONS"(.*?)', manpage) is not None

    def parse_man_page(self, manpage):
        options_section_regex = re.compile(r'\.SH "OPTIONS"(.*?)(\.SH|\Z)', re.DOTALL)
        options_section = re.search(options_section_regex, manpage).group(1)

        options_parts_regex = re.compile(r"\.PP(.*?)\.RE", re.DOTALL)
        options_matched = re.search(options_parts_regex, options_section)
        add_diagnostic("Command is %r" % CMDNAME)

        if options_matched is None:
            add_diagnostic("Unable to find options")
            if self.fallback(options_section):
                return True
            elif self.fallback2(options_section):
                return True
            return False

        while options_matched is not None:
            data = options_matched.group(1)
            last_dotpp_index = data.rfind(".PP")
            if last_dotpp_index != -1:
                data = data[last_dotpp_index + 3 :]

            data = remove_groff_formatting(data)
            data = data.split(".RS 4")
            if len(data) > 1:  # and len(data[1]) <= 300):
                optionName = data[0].strip()

                if optionName.find("-") == -1:
                    add_diagnostic("%r doesn't contain '-' " % optionName)
                else:
                    optionName = unquote_double_quotes(optionName)
                    optionName = unquote_single_quotes(optionName)
                    optionDescription = data[1].strip().replace("\n", " ")
                    built_command(optionName, optionDescription)

            else:
                add_diagnostic("Unable to split option from description")
                return False

            options_section = options_section[options_matched.end() - 3 :]
            options_matched = re.search(options_parts_regex, options_section)

    def fallback(self, options_section):
        add_diagnostic("Trying fallback")
        options_parts_regex = re.compile(r"\.TP( \d+)?(.*?)\.TP", re.DOTALL)
        options_matched = re.search(options_parts_regex, options_section)
        if options_matched is None:
            add_diagnostic("Still not found")
            return False
        while options_matched is not None:
            data = options_matched.group(2)
            data = remove_groff_formatting(data)
            data = data.strip()
            data = data.split("\n", 1)
            if len(data) > 1 and len(data[1].strip()) > 0:  # and len(data[1])<400):
                optionName = data[0].strip()
                if optionName.find("-") == -1:
                    add_diagnostic("%r doesn't contain '-'" % optionName)
                else:
                    optionName = unquote_double_quotes(optionName)
                    optionName = unquote_single_quotes(optionName)
                    optionDescription = data[1].strip().replace("\n", " ")
                    built_command(optionName, optionDescription)
            else:
                add_diagnostic("Unable to split option from description")
                return False

            options_section = options_section[options_matched.end() - 3 :]
            options_matched = re.search(options_parts_regex, options_section)
        return True

    def fallback2(self, options_section):
        add_diagnostic("Trying last chance fallback")
        ix_remover_regex = re.compile(r"\.IX.*")
        trailing_num_regex = re.compile("\\d+$")
        options_parts_regex = re.compile(r"\.IP (.*?)\.IP", re.DOTALL)

        options_section = re.sub(ix_remover_regex, "", options_section)
        options_matched = re.search(options_parts_regex, options_section)
        if options_matched is None:
            add_diagnostic("Still (still!) not found")
            return False
        while options_matched is not None:
            data = options_matched.group(1)

            data = remove_groff_formatting(data)
            data = data.strip()
            data = data.split("\n", 1)
            if len(data) > 1 and len(data[1].strip()) > 0:  # and len(data[1])<400):
                optionName = re.sub(trailing_num_regex, "", data[0].strip())

                if "-" not in optionName:
                    add_diagnostic("%r doesn't contain '-'" % optionName)
                else:
                    optionName = optionName.strip()
                    optionName = unquote_double_quotes(optionName)
                    optionName = unquote_single_quotes(optionName)
                    optionDescription = data[1].strip().replace("\n", " ")
                    built_command(optionName, optionDescription)
            else:
                add_diagnostic("Unable to split option from description")
                return False

            options_section = options_section[options_matched.end() - 3 :]
            options_matched = re.search(options_parts_regex, options_section)
        return True


class Type2ManParser(ManParser):
    def is_my_type(self, manpage):
        return compile_and_search(r"\.SH OPTIONS(.*?)", manpage) is not None

    def parse_man_page(self, manpage):
        options_section_regex = re.compile(r"\.SH OPTIONS(.*?)(\.SH|\Z)", re.DOTALL)
        options_section = re.search(options_section_regex, manpage).group(1)

        options_parts_regex = re.compile(
            r"\.[IT]P( \d+(\.\d)?i?)?(.*?)\.([IT]P|UNINDENT|UN|SH)", re.DOTALL
        )
        options_matched = re.search(options_parts_regex, options_section)
        add_diagnostic("Command is %r" % CMDNAME)

        if options_matched is None:
            add_diagnostic("%r: Unable to find options" % self)
            return False

        while options_matched is not None:
            data = options_matched.group(3)
            data = remove_groff_formatting(data)
            data = data.strip()
            data = data.split("\n", 1)

            if len(data) > 1 and len(data[1].strip()) > 0:  # and len(data[1])<400):
                optionName = data[0].strip()
                if "-" not in optionName:
                    add_diagnostic("%r doesn't contain '-'" % optionName)
                else:
                    optionName = unquote_double_quotes(optionName)
                    optionName = unquote_single_quotes(optionName)
                    optionDescription = data[1].strip().replace("\n", " ")
                    built_command(optionName, optionDescription)
            else:
                add_diagnostic("Unable to split option from description")

            options_section = options_section[options_matched.end() - 3 :]
            options_matched = re.search(options_parts_regex, options_section)


class Type3ManParser(ManParser):
    def is_my_type(self, manpage):
        return compile_and_search(r"\.SH DESCRIPTION(.*?)", manpage) is not None

    def parse_man_page(self, manpage):
        options_section_regex = re.compile(r"\.SH DESCRIPTION(.*?)(\.SH|\Z)", re.DOTALL)
        options_section = re.search(options_section_regex, manpage).group(1)

        options_parts_regex = re.compile(r"\.TP(.*?)\.TP", re.DOTALL)
        options_matched = re.search(options_parts_regex, options_section)
        add_diagnostic("Command is %r" % CMDNAME)

        if options_matched is None:
            add_diagnostic("Unable to find options section")
            return False

        while options_matched is not None:
            data = options_matched.group(1)

            data = remove_groff_formatting(data)
            data = data.strip()
            data = data.split("\n", 1)

            if len(data) > 1:  # and len(data[1])<400):
                optionName = data[0].strip()
                if optionName.find("-") == -1:
                    add_diagnostic("%r doesn't contain '-'" % optionName)
                else:
                    optionName = unquote_double_quotes(optionName)
                    optionName = unquote_single_quotes(optionName)
                    optionDescription = data[1].strip().replace("\n", " ")
                    built_command(optionName, optionDescription)

            else:
                add_diagnostic("Unable to split option from description")
                return False

            options_section = options_section[options_matched.end() - 3 :]
            options_matched = re.search(options_parts_regex, options_section)


class Type4ManParser(ManParser):
    def is_my_type(self, manpage):
        return compile_and_search(r"\.SH FUNCTION LETTERS(.*?)", manpage) is not None

    def parse_man_page(self, manpage):
        options_section_regex = re.compile(
            r"\.SH FUNCTION LETTERS(.*?)(\.SH|\Z)", re.DOTALL
        )
        options_section = re.search(options_section_regex, manpage).group(1)

        options_parts_regex = re.compile(r"\.TP(.*?)\.TP", re.DOTALL)
        options_matched = re.search(options_parts_regex, options_section)
        add_diagnostic("Command is %r" % CMDNAME)

        if options_matched is None:
            print("Unable to find options section", file=sys.stderr)
            return False

        while options_matched is not None:
            data = options_matched.group(1)

            data = remove_groff_formatting(data)
            data = data.strip()
            data = data.split("\n", 1)

            if len(data) > 1:  # and len(data[1])<400):
                optionName = data[0].strip()
                if optionName.find("-") == -1:
                    add_diagnostic("%r doesn't contain '-' " % optionName)
                else:
                    optionName = unquote_double_quotes(optionName)
                    optionName = unquote_single_quotes(optionName)
                    optionDescription = data[1].strip().replace("\n", " ")
                    built_command(optionName, optionDescription)

            else:
                add_diagnostic("Unable to split option from description")
                return False

            options_section = options_section[options_matched.end() - 3 :]
            options_matched = re.search(options_parts_regex, options_section)


class TypeScdocManParser(ManParser):
    def is_my_type(self, manpage):
        return (
            compile_and_search(
                r"\.(\\)(\") Generated by scdoc(.*?)\.SH OPTIONS(.*?)", manpage
            )
            is not None
        )

    def parse_man_page(self, manpage):
        options_section_regex = re.compile(r"\.SH OPTIONS(.*?)\.SH", re.DOTALL)
        options_section_matched = re.search(options_section_regex, manpage)
        if options_section_matched is None:
            return False
        options_section = options_section_matched.group(1)

        options_parts_regex = re.compile(r"(.*?)\.RE", re.DOTALL)
        options_matched = re.match(options_parts_regex, options_section)
        add_diagnostic("Command is %r" % CMDNAME)

        if options_matched is None:
            add_diagnostic("%r: Unable to find options" % self)
            return False

        while options_matched is not None:
            # Get first option and move options_section
            option = options_matched.group(1)
            options_section = options_section[options_matched.end() : :]
            options_matched = re.match(options_parts_regex, options_section)

            option = remove_groff_formatting(option)
            option = option.split("\n")

            # Crean option list
            option_clean = []
            for line in option:
                if line not in ["", ".P", ".RS 4"]:
                    option_clean.append(line)

            # Shold be at least two lines
            if len(option_clean) < 2:
                add_diagnostic("Unable to split option from description")
                continue

            # Name and description, others lines are ignored
            option_name = option_clean[0]
            option_description = option_clean[1]

            if "-" not in option_name:
                add_diagnostic("%r doesn't contain '-'" % option_name)

            option_name = unquote_double_quotes(option_name)
            option_name = unquote_single_quotes(option_name)
            built_command(option_name, option_description)

        return True


class TypeDarwinManParser(ManParser):
    def is_my_type(self, manpage):
        return compile_and_search(r"\.S[hH] DESCRIPTION", manpage) is not None

    def trim_groff(self, line):
        # Remove initial period
        if line.startswith("."):
            line = line[1:]
        # Skip leading groff crud
        while re.match(r"[A-Z][a-z]\s", line):
            line = line[3:]

        # If the line ends with a space and then a period or comma, then erase the space
        # This hack handles lines of the form '.Ar projectname .'
        if line.endswith(" ,") or line.endswith(" ."):
            line = line[:-2] + line[-1]
        return line

    def count_argument_dashes(self, line):
        # Determine how many dashes the line has using the following regex hack
        # Look for the start of a line, followed by a dot, then a sequence of
        # one or more dashes ('Fl')
        result = 0
        if line.startswith("."):
            line = line[4:]
            while line.startswith("Fl "):
                result = result + 1
                line = line[3:]
        return result

    # Replace some groff escapes. There's a lot we don't bother to handle.
    def groff_replace_escapes(self, line):
        line = line.replace(".Nm", CMDNAME)
        line = line.replace("\\ ", " ")
        line = line.replace(r"\& ", "")
        line = line.replace(".Pp", "")
        return line

    def is_option(self, line):
        return line.startswith(".It Fl")

    def parse_man_page(self, manpage):
        got_something = False
        lines = manpage.splitlines()
        # Discard lines until we get to ".sh Description"
        while lines and not (
            lines[0].startswith(".Sh DESCRIPTION")
            or lines[0].startswith(".SH DESCRIPTION")
        ):
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
            if not line:
                continue

            # Extract the name
            name = line.split(None, 2)[0]

            # Extract the description
            desc_lines = []
            while lines and not self.is_option(lines[0]):
                line = lines.pop(0).strip()
                # Ignore comments
                if line.startswith(r".\""):
                    continue
                if line.startswith("."):
                    line = self.groff_replace_escapes(line)
                    line = self.trim_groff(line).strip()
                if line:
                    desc_lines.append(line)
            desc = " ".join(desc_lines)

            if name == "-":
                # Skip double -- arguments
                continue
            elif len(name) > 1:
                # Output the command
                built_command(("-" * dash_count) + name, desc)
                got_something = True
            elif len(name) == 1:
                built_command("-" + name, desc)
                got_something = True

        return got_something


class TypeDeroffManParser(ManParser):
    def is_my_type(self, manpage):
        return True  # We're optimists

    def is_option(self, line):
        return line.startswith("-")

    def could_be_description(self, line):
        return len(line) > 0 and not line.startswith("-")

    def parse_man_page(self, manpage):
        d = Deroffer()
        d.deroff(manpage)
        output = d.get_output()
        lines = output.split("\n")

        got_something = False

        # Discard lines until we get to DESCRIPTION or OPTIONS
        while lines and not (
            lines[0].startswith("DESCRIPTION")
            or lines[0].startswith("OPTIONS")
            or lines[0].startswith("COMMAND OPTIONS")
        ):
            lines.pop(0)

        # Look for BUGS and stop there
        for idx in range(len(lines)):
            line = lines[idx]
            if line.startswith("BUGS"):
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
            description = ""
            while lines and self.could_be_description(lines[0]):
                if description:
                    description += " "
                description += lines.pop(0)

            built_command(options, description)
            got_something = True

        return got_something


# Return whether the file at the given path is overwritable
# Raises IOError if it cannot be opened
def file_is_overwritable(path):
    result = False
    file = open(path, "r", encoding="utf-8")
    for line in file:
        # Skip leading empty lines
        line = line.strip()
        if not line:
            continue

        # We look in the initial run of lines that start with #
        if not line.startswith("#"):
            break

        # See if this contains the magic word
        if "Autogenerated" in line:
            result = True
            break

    file.close()
    return result


# Remove any and all autogenerated completions in the given directory
def cleanup_autogenerated_completions_in_directory(dir):
    try:
        for filename in os.listdir(dir):
            # Skip non .fish files
            if not filename.endswith(".fish"):
                continue
            path = os.path.join(dir, filename)
            cleanup_autogenerated_file(path)
    except OSError:
        return False


# Delete the file if it is autogenerated
def cleanup_autogenerated_file(path):
    try:
        if file_is_overwritable(path):
            os.remove(path)
    except (OSError, IOError):
        pass


def parse_manpage_at_path(manpage_path, output_directory):
    # Return if CMDNAME is in 'ignoredcommands'
    ignoredcommands = [
        "cc",
        "g++",
        "gcc",
        "c++",
        "cpp",
        "emacs",
        "gprof",
        "wget",
        "ld",
        "awk",
    ]
    if CMDNAME in ignoredcommands:
        return

    # Ignore some commands' gazillion man pages
    # for subcommands - especially things we already have
    ignored_prefixes = [
        "bundle-",
        "cargo-",
        "ffmpeg-",
        "flatpak-",
        "gcloud_",
        "git-",
        "jj-",
        "npm-",
        "openssl-",
        "ostree-",
        "perf-",
        "perl",
        "pip-",
        "zsh",
    ]
    for prefix in ignored_prefixes:
        if CMDNAME.startswith(prefix):
            return

    # Clear diagnostics
    global diagnostic_indent
    diagnostic_output[:] = []
    diagnostic_indent = 0

    # Set up some diagnostics
    add_diagnostic("Considering " + manpage_path)
    diagnostic_indent += 1

    if manpage_path.endswith(".gz"):
        fd = gzip.open(manpage_path, "r")
        manpage = fd.read()
        manpage = manpage.decode("latin-1")
    elif manpage_path.endswith(".bz2"):
        fd = bz2.BZ2File(manpage_path, "r")
        manpage = fd.read()
        manpage = manpage.decode("latin-1")
    elif manpage_path.endswith(".xz") or manpage_path.endswith(".lzma"):
        if not lzma_available:
            return
        fd = lzma.LZMAFile(str(manpage_path), "r")
        manpage = fd.read()
        manpage = manpage.decode("latin-1")
    elif manpage_path.endswith((".1", ".2", ".3", ".4", ".5", ".6", ".7", ".8", ".9")):
        fd = open(manpage_path, "r", encoding="latin-1")
        manpage = fd.read()
    else:
        return
    fd.close()

    manpage = str(manpage)

    # Ignore the millions of links to BUILTIN(1)
    if "BUILTIN 1" in manpage or "builtin.1" in manpage:
        return

    # Clear the output list
    built_command_output[:] = []
    global already_output_completions
    already_output_completions = {}

    if DEROFF_ONLY:
        parsers = [TypeDeroffManParser()]
    else:
        parsers = [
            TypeScdocManParser(),
            Type1ManParser(),
            Type2ManParser(),
            Type4ManParser(),
            Type3ManParser(),
            TypeDarwinManParser(),
            TypeDeroffManParser(),
        ]
    parsersToTry = [p for p in parsers if p.is_my_type(manpage)]

    success = False
    for parser in parsersToTry:
        add_diagnostic("Trying %s" % parser.__class__.__name__)
        diagnostic_indent += 1
        success = parser.parse_man_page(manpage)
        diagnostic_indent -= 1
        # Make sure empty files aren't reported as success
        if not built_command_output:
            success = False
        if success:
            PARSER_INFO.setdefault(parser.__class__.__name__, []).append(CMDNAME)
            break

    if success:
        if WRITE_TO_STDOUT:
            output_file = sys.stdout
        else:
            fullpath = os.path.join(output_directory, CMDNAME + ".fish")
            try:
                output_file = open(fullpath, "w", encoding="utf-8")
            except IOError as err:
                add_diagnostic(
                    "Unable to open file '%s': error(%d): %s"
                    % (fullpath, err.errno, err.strerror)
                )
                return False

        # Output the magic word Autogenerated so we can tell if we can overwrite this
        built_command_output.insert(
            0, "# " + CMDNAME + "\n# Autogenerated from man page " + manpage_path
        )
        # built_command_output.insert(2, "# using " + parser.__class__.__name__) # XXX MISATTRIBUTES THE CULPABLE PARSER! Was really using Type2 but reporting TypeDeroffManParser

        for line in built_command_output:
            output_file.write(line)
            output_file.write("\n")
        output_file.write("\n")
        add_diagnostic(manpage_path + " parsed successfully")
        if output_file != sys.stdout:
            output_file.close()
    else:
        parser_names = ", ".join(p.__class__.__name__ for p in parsersToTry)
        # add_diagnostic('%s contains no options or is unparsable' % manpage_path, BRIEF_VERBOSE)
        add_diagnostic(
            "%s contains no options or is unparsable (tried parser %s)"
            % (manpage_path, parser_names),
            BRIEF_VERBOSE,
        )

    return success


def parse_and_output_man_pages(paths, output_directory, show_progress):
    global diagnostic_indent, CMDNAME
    paths.sort()
    total_count = len(paths)
    successful_count, index = 0, 0
    padding_len = len(str(total_count))
    last_progress_string_length = 0
    if show_progress and not WRITE_TO_STDOUT:
        print(
            "Parsing man pages and writing completions to {0}".format(output_directory)
        )

    man_page_suffixes = set([os.path.splitext(m)[1][1:] for m in paths])
    lzma_xz_occurs = "xz" in man_page_suffixes or "lzma" in man_page_suffixes
    if lzma_xz_occurs and not lzma_available:
        add_diagnostic(
            'At least one man page is compressed with lzma or xz, but the "lzma" module is not available.'
            " Any man page compressed with either will be skipped.",
            NOT_VERBOSE,
        )
        flush_diagnostics(sys.stderr)

    for manpage_path in paths:
        index += 1

        # Get the "base" command, e.g. mkfs.xfs.8.gz -> mkfs.xfs
        man_file_name = os.path.basename(manpage_path)
        # 1. strip the optional compressor suffix
        CMDNAME, extension = os.path.splitext(man_file_name)
        # 2. strip mandatory section nu:ber
        if CMDNAME.endswith((".1", ".2", ".3", ".4", ".5", ".6", ".7", ".8", ".9")):
            CMDNAME, extension = os.path.splitext(CMDNAME)
        # 3. XXX strip optional version(?) number?
        # see comment above `already_output_completions = {}` line

        # Show progress if we're doing that
        if show_progress:
            progress_str = "  {0} / {1} : {2}".format(
                (str(index).rjust(padding_len)), total_count, man_file_name
            )
            # Pad on the right with spaces so we overwrite whatever we wrote last time
            padded_progress_str = progress_str.ljust(last_progress_string_length)
            last_progress_string_length = len(progress_str)
            sys.stdout.write("\r{0}\r".format(padded_progress_str))
            sys.stdout.flush()

        try:
            if parse_manpage_at_path(manpage_path, output_directory):
                successful_count += 1
        except IOError:
            diagnostic_indent = 0
            add_diagnostic("Cannot open " + manpage_path)
        except (KeyboardInterrupt, SystemExit):
            raise
        except:
            add_diagnostic(
                "Error parsing %s: %s" % (manpage_path, sys.exc_info()[0]),
                BRIEF_VERBOSE,
            )
            flush_diagnostics(sys.stderr)
            traceback.print_exc(file=sys.stderr)
        flush_diagnostics(sys.stderr)
    print("")  # Newline after loop
    add_diagnostic(
        "Successfully parsed %d / %d pages" % (successful_count, total_count),
        BRIEF_VERBOSE,
    )
    flush_diagnostics(sys.stderr)


def get_paths_from_man_locations():
    # Return all the paths to man(1) and man(8) files in the manpath

    parent_paths = []

    # Most (GNU, macOS, Haiku) modern implementations of man support being called with `--path`.
    # Traditional implementations require a second `manpath` program: examples include FreeBSD and Solaris.
    # Prefer an external program first because these programs return a superset of the $MANPATH variable.
    for prog in [["man", "--path"], ["manpath"]]:
        try:
            output = subprocess.check_output(prog, stderr=DEVNULL)
            output = output.decode("latin-1")
            parent_paths = output.strip().split(":")
            break
        except (OSError, subprocess.CalledProcessError):
            continue
    # If we can't have the OS interpret $MANPATH, just use it as-is (gulp).
    if not parent_paths and os.getenv("MANPATH"):
        parent_paths = os.getenv("MANPATH").strip().split(":")
    # Fallback: With mandoc (OpenBSD, embedded Linux) and NetBSD man, the only way to get the default manpath is by reading /etc.
    if not parent_paths:
        try:
            with open("/etc/man.conf", "r") as file:
                data = file.read()
                for key in ["MANPATH", "_default"]:
                    # novermin
                    for match in re.findall(r"^%s\s+(.*)$" % key, data, re.I | re.M):
                        parent_paths.append(match)
        except FileNotFoundError:
            pass
    # Fallback: hard-code some common paths. These should be likely for FHS Linux distros, BSDs, and macOS.
    if not parent_paths:
        parent_paths = ["/usr/share/man", "/usr/local/man", "/usr/local/share/man"]
        print(
            "Unable to get the manpath, falling back to %s." % ":".join(parent_paths),
            "Explictly set $MANPATH to fix this error.",
            file=sys.stderr,
        )

    result = []
    for parent_path in parent_paths:
        for section in ["man1", "man6", "man8"]:
            directory_path = os.path.join(parent_path, section)
            try:
                names = os.listdir(directory_path)
            except OSError:
                names = []
            names.sort()
            for name in names:
                result.append(os.path.join(directory_path, name))
    return result


if __name__ == "__main__":
    script_name = sys.argv[0]
    parser = argparse.ArgumentParser(
        description="create_manpage_completions: Generate fish-shell completions from manpages"
    )
    parser.add_argument(
        "-c",
        "--cleanup-in",
        type=str,
        help="Directories to clean up in",
        action="append",
    )
    parser.add_argument(
        "-d",
        "--directory",
        type=str,
        help="The directory to save the completions in",
    )
    parser.add_argument(
        "-k",
        "--keep",
        help="Whether to keep files in the target directory",
        action="store_true",
    )
    parser.add_argument(
        "-m",
        "--manpath",
        help="Whether to use manpath",
        action="store_true",
    )
    parser.add_argument(
        "-p",
        "--progress",
        help="Whether to show progress",
        action="store_true",
    )
    parser.add_argument(
        "-s",
        "--stdout",
        help="Write the completions to stdout",
        action="store_true",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        type=int,
        choices=[0, 1, 2],
        help="The level of debug output to show",
    )
    parser.add_argument(
        "-z",
        "--deroff-only",
        help="Whether to just deroff",
        action="store_true",
    )
    parser.add_argument("file_paths", type=str, nargs="*")

    args = parser.parse_args()

    if args.verbose:
        VERBOSITY = args.verbose
    if args.stdout:
        WRITE_TO_STDOUT = True
    if args.deroff_only:
        DEROFF_ONLY = True
    if args.keep:
        KEEP_FILES = True
    if args.manpath:
        # Fetch all man1 and man8 files from the manpath or man.conf
        args.file_paths.extend(get_paths_from_man_locations())

    # Directories within which we will clean up autogenerated completions
    # This script originally wrote completions into ~/.config/fish/completions
    # Now it writes them into a separate directory
    if args.cleanup_in:
        for cleanup_dir in args.cleanup_in:
            cleanup_autogenerated_completions_in_directory(cleanup_dir)

    if not args.file_paths:
        if args.manpath:
            print("No paths specified and I can't make sense of your MANPATH")
            print("Please either give paths or set $MANPATH and try again")
        else:
            print("No paths specified")
        sys.exit(0)

    if not args.stdout and not args.directory:
        # Default to ~/.cache/fish/generated_completions
        # Create it if it doesn't exist
        xdg_cache_home = os.getenv("XDG_CACHE_HOME", "~/.cache")
        args.directory = os.path.expanduser(
            xdg_cache_home + "/fish/generated_completions/"
        )
        try:
            os.makedirs(args.directory)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise

    if not args.stdout and not args.keep:
        # Remove old generated files
        cleanup_autogenerated_completions_in_directory(args.directory)

    parse_and_output_man_pages(args.file_paths, args.directory, args.progress)
