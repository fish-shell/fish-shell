use crate::prelude::*;
use crate::reader::is_backslashed;
use crate::tokenizer::tok_is_string_character;

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum MoveWordStyle {
    /// stop at punctuation
    Punctuation,
    /// stops at path components
    PathComponents,
    /// stops at whitespace
    Whitespace,
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum MoveWordDir {
    Left,
    Right,
}

pub struct MoveWordStateMachine {
    state: MoveWordState,
}

enum MoveWordState {
    SmallWord(State<SmallWordMovementState>),
    PathComponent(State<PathComponentState>),
    BigWord(State<BigWordMovementState>),
}

enum State<S: HasNextState> {
    Initial,
    Live(S),
    Final,
}

enum NonFinalState<S> {
    Initial,
    Live(S),
}

trait HasNextState {
    fn next_state(state: NonFinalState<&Self>, text: &wstr, idx: usize, c: char) -> State<Self>
    where
        Self: Sized;
}

impl MoveWordStateMachine {
    pub fn new(style: MoveWordStyle) -> Self {
        use MoveWordState as MWS;
        use MoveWordStyle as Style;
        let state = match style {
            Style::Punctuation => MWS::SmallWord(State::Initial),
            Style::PathComponents => MWS::PathComponent(State::Initial),
            Style::Whitespace => MWS::BigWord(State::Initial),
        };
        Self { state }
    }
    pub fn consume_char(&mut self, text: &wstr, idx: usize) -> bool {
        use MoveWordState as MWS;
        match &mut self.state {
            MWS::SmallWord(state) => Self::to_next_state(state, text, idx),
            MWS::PathComponent(state) => Self::to_next_state(state, text, idx),
            MWS::BigWord(state) => Self::to_next_state(state, text, idx),
        }
    }
    fn to_next_state<MWS: HasNextState>(state: &mut State<MWS>, text: &wstr, idx: usize) -> bool {
        let input_state = match &*state {
            State::Initial => NonFinalState::Initial,
            State::Live(s) => NonFinalState::Live(s),
            State::Final => panic!(),
        };
        let c = text.as_char_slice()[idx];
        *state = MWS::next_state(input_state, text, idx, c);
        !matches!(*state, State::Final)
    }
}

#[derive(Clone, Copy)]
enum SmallWordMovementState {
    Rest,
    WhitespaceRest,
    Whitespace,
    Alphanumeric,
}
impl HasNextState for SmallWordMovementState {
    fn next_state(state: NonFinalState<&Self>, _text: &wstr, _idx: usize, c: char) -> State<Self> {
        use {NonFinalState as NFS, SmallWordMovementState as S};
        let final_state = State::Final;
        State::Live(match state {
            NFS::Initial => {
                if c.is_whitespace() {
                    S::Whitespace
                } else if c.is_alphanumeric() {
                    S::Alphanumeric
                } else {
                    // Don't allow switching type (ws->nonws) after non-whitespace and
                    // non-alphanumeric.
                    S::Rest
                }
            }
            NFS::Live(S::Rest) => {
                if c.is_whitespace() {
                    // Consume only trailing whitespace.
                    S::WhitespaceRest
                } else if c.is_alphanumeric() {
                    // Consume only alnums.
                    S::Alphanumeric
                } else {
                    return final_state;
                }
            }
            NFS::Live(s @ S::WhitespaceRest) | NFS::Live(s @ S::Whitespace) => {
                // "whitespace" consumes whitespace and switches to alnums,
                // "whitespace_rest" only consumes whitespace.
                if c.is_whitespace() {
                    // Consumed whitespace.
                    *s
                } else {
                    if !matches!(s, S::Whitespace) {
                        return final_state;
                    }
                    if c.is_alphanumeric() {
                        S::Alphanumeric
                    } else {
                        return final_state;
                    }
                }
            }
            NFS::Live(S::Alphanumeric) => {
                if c.is_alphanumeric() {
                    S::Alphanumeric
                } else {
                    return final_state;
                }
            }
        })
    }
}

// Consume a "word" of printable characters plus any leading whitespace.
enum BigWordMovementState {
    Blank,
    Graph,
}
impl HasNextState for BigWordMovementState {
    fn next_state(state: NonFinalState<&Self>, _text: &wstr, _idx: usize, c: char) -> State<Self> {
        use {BigWordMovementState as S, NonFinalState as NFS};
        State::Live(match state {
            NFS::Initial => {
                // always consume the first character
                // If it's not whitespace, only consume those from here.
                if !c.is_whitespace() {
                    S::Graph
                } else {
                    // If it's whitespace, keep consuming whitespace until the graphs.
                    S::Blank
                }
            }
            NFS::Live(S::Blank) => {
                if c.is_whitespace() {
                    // consumed whitespace
                    S::Blank
                } else {
                    S::Graph
                }
            }
            NFS::Live(S::Graph) => {
                if !c.is_whitespace() {
                    // consumed printable non-space
                    S::Graph
                } else {
                    return State::Final;
                }
            }
        })
    }
}

fn is_path_component_character(c: char) -> bool {
    tok_is_string_character(c, None) && !L!("/={,}'\":@#").as_char_slice().contains(&c)
}

/// a path component contains either
/// - [space +] end word
/// - punc + end word
/// - space + end punc
#[derive(Clone, Copy)]
enum PathComponentState {
    Whitespace,
    Separator,
    Slash,
    PathComponentCharacters,
    InitialSeparator,
}
impl HasNextState for PathComponentState {
    fn next_state(state: NonFinalState<&Self>, text: &wstr, idx: usize, c: char) -> State<Self> {
        use {NonFinalState as NFS, PathComponentState as S};
        let is_escaped = is_backslashed(text, idx);
        let is_whitespace = c.is_whitespace() && !is_escaped;
        let is_path_component_character =
            is_path_component_character(c) || (c.is_whitespace() && is_escaped);

        let final_state = State::Final;

        State::Live(match state {
            NFS::Initial => {
                if !is_path_component_character && !is_whitespace {
                    S::InitialSeparator
                } else {
                    if is_path_component_character {
                        S::PathComponentCharacters
                    } else {
                        S::Whitespace
                    }
                }
            }
            NFS::Live(S::Whitespace) => {
                if is_whitespace {
                    S::Whitespace
                } else if c == '/' {
                    S::Slash // path component
                } else if is_path_component_character {
                    S::PathComponentCharacters
                } else {
                    S::Separator // path separator
                }
            }
            NFS::Live(S::Separator) => {
                if !is_whitespace && !is_path_component_character {
                    S::Separator
                } else {
                    return final_state;
                }
            }
            NFS::Live(S::Slash) => {
                if c == '/' {
                    S::Slash
                } else {
                    S::PathComponentCharacters
                }
            }
            NFS::Live(S::PathComponentCharacters) => {
                if is_path_component_character {
                    S::PathComponentCharacters
                } else {
                    return final_state;
                }
            }
            NFS::Live(S::InitialSeparator) => {
                if is_path_component_character {
                    S::PathComponentCharacters
                } else if is_whitespace {
                    return final_state;
                } else {
                    S::InitialSeparator
                }
            }
        })
    }
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
                let new_sm = || MoveWordStateMachine::new($style);
                let mut sm = new_sm();
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
                        sm = new_sm();
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
