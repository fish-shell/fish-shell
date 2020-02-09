# This is a plugin for pygments that shells out to fish_indent.

# Example of how to use this:
# env PATH="/dir/containing/fish/indent/:$PATH" pygmentize -f terminal256 -l /path/to/fish_indent_lexer.py:FishIndentLexer -x ~/test.fish

import os
from pygments.lexer import Lexer
from pygments.token import (
    Keyword,
    Name,
    Comment,
    String,
    Error,
    Number,
    Operator,
    Other,
    Generic,
    Whitespace,
    String,
    Text,
    Punctuation,
)
import re
import subprocess

# The token type representing output to the console.
OUTPUT_TOKEN = Text

# A fallback token type.
DEFAULT = Text

# Mapping from fish token types to Pygments types.
ROLE_TO_TOKEN = {
    "normal": Name.Variable,
    "error": Generic.Error,
    "command": Name.Function,
    "statement_terminator": Punctuation,
    "param": Name.Constant,
    "comment": Comment,
    "match": DEFAULT,
    "search_match": DEFAULT,
    "operat": Operator,
    "escape": String.Escape,
    "quote": String.Single,  # note, may be changed to double dynamically
    "redirection": Punctuation,  # ?
    "autosuggestion": Other,  # in practice won't be generated
    "selection": DEFAULT,
    "pager_progress": DEFAULT,
    "pager_background": DEFAULT,
    "pager_prefix": DEFAULT,
    "pager_completion": DEFAULT,
    "pager_description": DEFAULT,
    "pager_secondary_background": DEFAULT,
    "pager_secondary_prefix": DEFAULT,
    "pager_secondary_completion": DEFAULT,
    "pager_secondary_description": DEFAULT,
    "pager_selected_background": DEFAULT,
    "pager_selected_prefix": DEFAULT,
    "pager_selected_completion": DEFAULT,
    "pager_selected_description": DEFAULT,
}


def token_for_text_and_role(text, role):
    """ Return the pygments token for some input text and a fish role
    
        This applies any special cases of ROLE_TO_TOKEN.
    """
    if text.isspace():
        # Here fish will return 'normal' or 'statement_terminator' for newline.
        return Text.Whitespace
    elif role == "quote":
        # Check for single or double.
        return String.Single if text.startswith("'") else String.Double
    else:
        return ROLE_TO_TOKEN[role]


def tokenize_fish_command(code, offset):
    """ Tokenize some fish code, offset in a parent string, by shelling
        out to fish_indent.
        
        fish_indent will output a list of csv lines: start,end,type.

        This function returns a list of (start, tok, value) tuples, as
        Pygments expects.
    """
    proc = subprocess.Popen(
        ["fish_indent", "--pygments"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        universal_newlines=False,
    )
    stdout, _ = proc.communicate(code.encode("utf-8"))
    result = []
    for line in stdout.decode("utf-8").splitlines():
        start, end, role = line.split(",")
        start, end = int(start), int(end)
        value = code[start:end]
        tok = token_for_text_and_role(value, role)
        result.append((start + offset, tok, value))
    return result


class FishIndentLexer(Lexer):
    name = "FishIndentLexer"
    aliases = ["fish", "fish-docs-samples"]
    filenames = ["*.fish"]

    def get_tokens_unprocessed(self, input_text):
        """ Return a list of (start, tok, value) tuples.

            start is the index into the string
            tok is the token type (as above)
            value is the string contents of the token
        """
        result = []
        if not any(s.startswith(">") for s in input_text.splitlines()):
            # No prompt, just tokenize everything.
            result = tokenize_fish_command(input_text, 0)
        else:
            # We have a prompt line.
            # Use a regexp because it will maintain string indexes for us.
            regex = re.compile(r"^(>_?\s*)?(.*\n?)", re.MULTILINE)
            for m in regex.finditer(input_text):
                if m.group(1):
                    # Prompt line; highlight via fish syntax.
                    result.append((m.start(1), Generic.Prompt, m.group(1)))
                    result.extend(tokenize_fish_command(m.group(2), m.start(2)))
                else:
                    # Non-prompt line representing output from a command.
                    result.append((m.start(2), OUTPUT_TOKEN, m.group(2)))
        return result
