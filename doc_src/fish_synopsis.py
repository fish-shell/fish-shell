# Pygments lexer for a fish command synopsis.
#
# Example usage:
# echo 'string match [OPTIONS] [STRING]' | pygmentize -f terminal256 -l doc_src/fish_synopsis.py:FishSynopsisLexer -x

from docutils import nodes
from pygments.lexer import Lexer
from pygments.token import (
    Generic,
    Name,
    Operator,
    Punctuation,
    Text,
)
import re
from sphinx.directives.code import CodeBlock


class FishSynopsisDirective(CodeBlock):
    """A custom directive that describes a command's grammar."""

    has_content = True
    required_arguments = 0

    def run(self):
        if self.env.app.builder.name != "man":
            self.arguments = ["fish-synopsis"]
            return CodeBlock.run(self)
        lexer = FishSynopsisLexer()
        result = nodes.line_block()
        for start, tok, text in lexer.get_tokens_unprocessed("\n".join(self.content)):
            if (  # Literal text.
                (tok in (Name.Function, Name.Constant) and not text.isupper())
                or text.startswith("-")  # Literal option, even if it's uppercase.
                or tok in (Operator, Punctuation)
                or text
                == " ]"  # Tiny hack: the closing bracket of the test(1) alias is a literal.
            ):
                node = nodes.strong(text=text)
            elif (
                tok in (Name.Constant, Name.Function) and text.isupper()
            ):  # Placeholder parameter.
                node = nodes.emphasis(text=text)
            else:  # Grammar metacharacter or whitespace.
                node = nodes.inline(text=text)
            result.append(node)
        return [result]


lexer_rules = [
    (re.compile(pattern), token)
    for pattern, token in (
        # Hack: treat the "[ expr ]" alias of builtin test as command token (not as grammar
        # metacharacter).  This works because we write it without spaces in the grammar (like
        # "[OPTIONS]").
        (r"\. |! |\[ | \]|\{ | \}", Name.Constant),
        # Statement separators.
        (r"\n", Text.Whitespace),
        (r";", Punctuation),
        (r" +", Text.Whitespace),
        # Operators have different highlighting than commands or parameters.
        (r"\b(and|not|or|time)\b", Operator),
        # Keywords that are not in command position.
        (r"\b(if|in)\b", Name.Function),
        # Grammar metacharacters.
        (r"[()[\]|]", Generic.Other),
        (r"\.\.\.", Generic.Other),
        # Parameters.
        (r"[\w-]+", Name.Constant),
        (r"[=%]", Name.Constant),
        (
            r"[<>]",
            Name.Constant,
        ),  # Redirection are highlighted like parameters by default.
    )
]


class FishSynopsisLexer(Lexer):
    name = "FishSynopsisLexer"
    aliases = ["fish-synopsis"]

    is_before_command_token = None

    def next_token(self, rule: str, offset: int, has_continuation_line: bool):
        for pattern, token_kind in lexer_rules:
            m = pattern.match(rule, pos=offset)
            if m is None:
                continue
            if token_kind is Name.Constant and self.is_before_command_token:
                token_kind = Name.Function

            if has_continuation_line:
                # Traditional case: rules with continuation lines only have a single command.
                self.is_before_command_token = False
            else:
                if m.group() in ("\n", ";") or token_kind is Operator:
                    self.is_before_command_token = True
                elif token_kind in (Name.Constant, Name.Function):
                    self.is_before_command_token = False

            return m, token_kind, m.end()
        return None, None, offset

    def get_tokens_unprocessed(self, input_text):
        """Return a list of (start, tok, value) tuples.

        start is the index into the string
        tok is the token type (as above)
        value is the string contents of the token
        """
        """
        A synopsis consists of multiple rules.  Each rule can have continuation lines, which
        are expected to be indented:

            cmd foo [--quux]
                    [ARGUMENT] ...
            cmd bar

        We'll split the input into rules. This is easy for a traditional synopsis because each
        non-indented line starts a new rule.  However, we also want to support code blocks:

            switch VALUE
               [case [GLOB ...]
                   [COMMAND ...]]
            end

        which makes this format ambiguous. Hack around this by always adding "end" to the
        current rule, which is enough in practice.
        """
        rules = []
        rule = []
        for line in list(input_text.splitlines()) + [""]:
            if rule and not line.startswith(" "):
                rules.append(rule)
                rule = []
                if line == "end":
                    rules[-1].append(line)
                    continue
            rule.append(line)
        result = []
        for rule in rules:
            offset = 0
            self.is_before_command_token = True
            has_continuation_line = rule[-1].startswith(" ")
            rule = "\n".join(rule) + "\n"
            while True:
                match, token_kind, offset = self.next_token(
                    rule, offset, has_continuation_line
                )
                if match is None:
                    break
                text = match.group()
                result.append((match.start(), token_kind, text))
            assert offset == len(rule), "cannot tokenize leftover text: '{}'".format(
                rule[offset:]
            )
        return result
