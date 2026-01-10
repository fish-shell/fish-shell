use crate::tokenizer::tok_is_string_character;

use crate::prelude::*;
use crate::reader::is_backslashed;

#[derive(Clone, Copy, Eq, PartialEq)]
pub enum MoveWordDir {
    Left,
    Right,
}

pub enum MoveWordStyle {
    /// stop at punctuation
    Punctuation,
    /// stops at path components
    PathComponents,
    /// stops at whitespace
    Whitespace,
}

/// Our state machine that implements "one word" movement or erasure.
pub struct MoveWordStateMachine {
    state: u8,
    style: MoveWordStyle,
}

impl MoveWordStateMachine {
    pub fn new(style: MoveWordStyle) -> Self {
        MoveWordStateMachine { state: 0, style }
    }

    pub fn consume_char(&mut self, text: &wstr, idx: usize) -> bool {
        let c = text.as_char_slice()[idx];
        match self.style {
            MoveWordStyle::Punctuation => self.consume_char_punctuation(c),
            MoveWordStyle::PathComponents => self.consume_char_path_components(text, idx, c),
            MoveWordStyle::Whitespace => self.consume_char_whitespace(c),
        }
    }

    #[cfg(test)]
    fn reset(&mut self) {
        self.state = 0;
    }

    fn consume_char_punctuation(&mut self, c: char) -> bool {
        const S_ALWAYS_ONE: u8 = 0;
        const S_REST: u8 = 1;
        const S_WHITESPACE_REST: u8 = 2;
        const S_WHITESPACE: u8 = 3;
        const S_ALPHANUMERIC: u8 = 4;
        const S_END: u8 = 5;

        let mut consumed = false;
        while self.state != S_END && !consumed {
            match self.state {
                S_ALWAYS_ONE => {
                    // Always consume the first character.
                    consumed = true;
                    if c.is_whitespace() {
                        self.state = S_WHITESPACE;
                    } else if c.is_alphanumeric() {
                        self.state = S_ALPHANUMERIC;
                    } else {
                        // Don't allow switching type (ws->nonws) after non-whitespace and
                        // non-alphanumeric.
                        self.state = S_REST;
                    }
                }
                S_REST => {
                    if c.is_whitespace() {
                        // Consume only trailing whitespace.
                        self.state = S_WHITESPACE_REST;
                    } else if c.is_alphanumeric() {
                        // Consume only alnums.
                        self.state = S_ALPHANUMERIC;
                    } else {
                        consumed = false;
                        self.state = S_END;
                    }
                }
                S_WHITESPACE_REST | S_WHITESPACE => {
                    // "whitespace" consumes whitespace and switches to alnums,
                    // "whitespace_rest" only consumes whitespace.
                    if c.is_whitespace() {
                        // Consumed whitespace.
                        consumed = true;
                    } else {
                        self.state = if self.state == S_WHITESPACE {
                            S_ALPHANUMERIC
                        } else {
                            S_END
                        };
                    }
                }
                S_ALPHANUMERIC => {
                    if c.is_alphanumeric() {
                        consumed = true; // consumed alphanumeric
                    } else {
                        self.state = S_END;
                    }
                }
                _ => {}
            }
        }
        consumed
    }

    fn consume_char_path_components(&mut self, s: &wstr, idx: usize, c: char) -> bool {
        const S_INITIAL_PUNCTUATION: u8 = 0;
        const S_WHITESPACE: u8 = 1;
        const S_SEPARATOR: u8 = 2;
        const S_SLASH: u8 = 3;
        const S_PATH_COMPONENT_CHARACTERS: u8 = 4;
        const S_INITIAL_SEPARATOR: u8 = 5;
        const S_END: u8 = 6;

        let is_escaped = is_backslashed(s, idx);
        let is_whitespace = c.is_whitespace() && !is_escaped;
        let is_path_component_character =
            is_path_component_character(c) || (c.is_whitespace() && is_escaped);

        let mut consumed = false;
        while self.state != S_END && !consumed {
            match self.state {
                S_INITIAL_PUNCTUATION => {
                    if !is_path_component_character && !is_whitespace {
                        self.state = S_INITIAL_SEPARATOR;
                    } else {
                        if !is_path_component_character {
                            consumed = true;
                        }
                        self.state = S_WHITESPACE;
                    }
                }
                S_WHITESPACE => {
                    if is_whitespace {
                        consumed = true; // consumed whitespace
                    } else if c == '/' || is_path_component_character {
                        self.state = S_SLASH; // path component
                    } else {
                        self.state = S_SEPARATOR; // path separator
                    }
                }
                S_SEPARATOR => {
                    if !is_whitespace && !is_path_component_character {
                        consumed = true; // consumed separator
                    } else {
                        self.state = S_END;
                    }
                }
                S_SLASH => {
                    if c == '/' {
                        consumed = true; // consumed slash
                    } else {
                        self.state = S_PATH_COMPONENT_CHARACTERS;
                    }
                }
                S_PATH_COMPONENT_CHARACTERS => {
                    if is_path_component_character {
                        consumed = true; // consumed string character except slash
                    } else {
                        self.state = S_END;
                    }
                }
                S_INITIAL_SEPARATOR => {
                    if is_path_component_character {
                        consumed = true;
                        self.state = S_PATH_COMPONENT_CHARACTERS;
                    } else if is_whitespace {
                        self.state = S_END;
                    } else {
                        consumed = true;
                    }
                }
                _ => {}
            }
        }
        consumed
    }

    fn consume_char_whitespace(&mut self, c: char) -> bool {
        // Consume a "word" of printable characters plus any leading whitespace.
        const S_ALWAYS_ONE: u8 = 0;
        const S_BLANK: u8 = 1;
        const S_GRAPH: u8 = 2;
        const S_END: u8 = 3;

        let mut consumed = false;
        while self.state != S_END && !consumed {
            match self.state {
                S_ALWAYS_ONE => {
                    // always consume the first character
                    // If it's not whitespace, only consume those from here.
                    consumed = true;
                    if !c.is_whitespace() {
                        self.state = S_GRAPH;
                    } else {
                        // If it's whitespace, keep consuming whitespace until the graphs.
                        self.state = S_BLANK;
                    }
                }
                S_BLANK => {
                    if c.is_whitespace() {
                        // consumed whitespace
                        consumed = true;
                    } else {
                        self.state = S_GRAPH;
                    }
                }
                S_GRAPH => {
                    if !c.is_whitespace() {
                        // consumed printable non-space
                        consumed = true;
                    } else {
                        self.state = S_END;
                    }
                }
                _ => {}
            }
        }
        consumed
    }
}

fn is_path_component_character(c: char) -> bool {
    tok_is_string_character(c, None) && !L!("/={,}'\":@#").as_char_slice().contains(&c)
}

#[cfg(test)]
mod tests {
    use super::{MoveWordDir, MoveWordStateMachine, MoveWordStyle};
    use crate::prelude::*;
    use std::collections::VecDeque;

    #[test]
    fn test_word_motion() {
        fn setup(direction: MoveWordDir, line: &str) -> (WString, VecDeque<usize>, usize, usize) {
            let mut command = WString::new();
            let mut stops = VecDeque::new();

            // Carets represent stops and should be cut out of the command.
            for c in line.chars() {
                if c == '^' {
                    if direction == MoveWordDir::Left {
                        stops.push_front(command.len());
                    } else {
                        stops.push_back(command.len());
                    }
                } else {
                    command.push(c);
                }
            }
            stops.pop_back();

            let idx = stops.pop_front().unwrap();
            let end = if direction == MoveWordDir::Left {
                0
            } else {
                command.len()
            };

            (command, stops, idx, end)
        }

        macro_rules! validate {
             ($direction:expr, $style:expr, $line:expr) => {
                let direction = $direction;
                let (command, mut stops, mut idx, end) = setup(direction, $line);
                assert!(!command.is_empty());
                let mut sm = MoveWordStateMachine::new($style);
                while idx != end {
                    let word_idx = if direction == MoveWordDir::Left {
                        idx - 1
                    } else {
                        idx
                    };
                    let consumed = sm.consume_char(&command, word_idx);
                    if consumed {
                        idx = if direction == MoveWordDir::Left {
                            idx - 1
                        } else {
                            idx + 1
                        };
                    } else {
                        assert!(
                            !stops.is_empty(),
                            "unexpected stop at {idx}. String: {command:?}"
                        );
                        let expected_idx = stops.front().unwrap();
                        assert_eq!(
                            idx, *expected_idx,
                            "Expected to stop={expected_idx} but stopped at {idx}. String: {command:?}"
                        );
                        stops.pop_front();
                        sm.reset();
                    }
                }
                assert!(
                    stops.is_empty(),
                    "expected to stop at {stops:?} but not. String: {command:?}"
                );
             }
        }

        use MoveWordDir::*;
        use MoveWordStyle::*;

        // PathComponents tests
        validate!(
            Left,
            PathComponents,
            "^echo ^/^foo/^bar{^aaa,^bbb,^ccc}^bak/^"
        );
        validate!(Left, PathComponents, "^echo ^bak ^///^");
        validate!(Left, PathComponents, "^aaa ^@ ^@^aaa^");
        validate!(Left, PathComponents, "^aaa ^a ^@^aaa^");
        validate!(Left, PathComponents, "^aaa ^@@@ ^@@^aa^");
        validate!(Left, PathComponents, "^aa^@@  ^aa@@^a^");
        validate!(Left, PathComponents, r#"^a\  ^b\ c/^d"^e\ f"^g"#);
        validate!(Left, PathComponents, r#"^a\"^bc^"#);

        validate!(Left, PathComponents, "^/^foo/^bar/^baz/^");
        validate!(Left, PathComponents, "^echo ^--foo ^--bar^");
        validate!(Left, PathComponents, "^echo ^hi ^> ^/^dev/^null^");

        // General punctuation tests
        validate!(Right, Punctuation, "^a^ bcd^");
        validate!(Right, Punctuation, "a^b^ cde^");
        validate!(Right, Punctuation, "^ab^ cde^");
        validate!(Right, Punctuation, "^ab^&cd^ ^& ^e^ f^&^");

        validate!(Left, Punctuation, "^echo ^hello_^world.^txt^");
        validate!(Right, Punctuation, "^echo^ hello^_world^.txt^");

        validate!(Left, Punctuation, "^echo ^foo_^foo_^foo/^/^/^/^/^    ^");
        validate!(Right, Punctuation, "^echo^ foo^_foo^_foo^/^/^/^/^/    ^");

        // General whitespace tests
        validate!(Right, Whitespace, "^a-b-c^ d-e-f^");
        validate!(Right, Whitespace, "^a-b-c^\n d-e-f^ ^");
        validate!(Right, Whitespace, "^a-b-c^\n\nd-e-f^ ^");
    }
}
