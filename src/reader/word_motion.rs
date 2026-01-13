use std::ops::ControlFlow;

use crate::prelude::*;
use crate::reader::is_backslashed;
use crate::tokenizer::tok_is_string_character;
use fish_wchar::word_char::{WordCharClass, is_blank};

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
    direction: MoveWordDir,
    state: MoveWordState,
}

enum MoveWordState {
    SmallWord(State<SmallWordMovementState>),
    PathComponentBackward(State<PathComponentBackwardState>),
    PathComponentForward(State<PathComponentForwardState>),
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
    fn next_state(
        direction: MoveWordDir,
        state: NonFinalState<&Self>,
        text: &wstr,
        idx: usize,
        c: char,
    ) -> State<Self>
    where
        Self: Sized;
}

impl MoveWordStateMachine {
    pub fn new(style: MoveWordStyle, direction: MoveWordDir) -> Self {
        use MoveWordState as MWS;
        use MoveWordStyle as Style;
        let state = match style {
            Style::Punctuation => MWS::SmallWord(State::Initial),
            Style::PathComponents => match direction {
                MoveWordDir::Left => MWS::PathComponentBackward(State::Initial),
                MoveWordDir::Right => MWS::PathComponentForward(State::Initial),
            },
            Style::Whitespace => MWS::BigWord(State::Initial),
        };
        Self { direction, state }
    }
    pub fn consume_char(&mut self, text: &wstr, idx: usize) -> bool {
        use MoveWordState as MWS;
        let direction = self.direction;
        match &mut self.state {
            MWS::SmallWord(state) => Self::to_next_state(direction, state, text, idx),
            MWS::PathComponentBackward(state) => Self::to_next_state(direction, state, text, idx),
            MWS::PathComponentForward(state) => Self::to_next_state(direction, state, text, idx),
            MWS::BigWord(state) => Self::to_next_state(direction, state, text, idx),
        }
    }
    fn to_next_state<MWS: HasNextState>(
        direction: MoveWordDir,
        state: &mut State<MWS>,
        text: &wstr,
        idx: usize,
    ) -> bool {
        let input_state = match &*state {
            State::Initial => NonFinalState::Initial,
            State::Live(s) => NonFinalState::Live(s),
            State::Final => panic!(),
        };
        let c = text.as_char_slice()[idx];
        *state = MWS::next_state(direction, input_state, text, idx, c);
        !matches!(*state, State::Final)
    }
}

trait WordMovementState: HasNextState {
    fn from_last_char_class(char_class: WordCharClass) -> Self;
    fn last_char_class(&self) -> WordCharClass;
}

struct SmallWordMovementState {
    last_char_class: WordCharClass,
}
impl WordMovementState for SmallWordMovementState {
    fn from_last_char_class(last_char_class: WordCharClass) -> Self {
        Self { last_char_class }
    }
    fn last_char_class(&self) -> WordCharClass {
        self.last_char_class
    }
}
impl HasNextState for SmallWordMovementState {
    fn next_state(
        direction: MoveWordDir,
        state: NonFinalState<&Self>,
        _text: &wstr,
        _idx: usize,
        c: char,
    ) -> State<Self> {
        let char_class = WordCharClass::from_char(c);
        next_word_movement_state(direction, state, char_class)
    }
}

struct BigWordMovementState {
    last_char_class: WordCharClass,
}
impl HasNextState for BigWordMovementState {
    fn next_state(
        direction: MoveWordDir,
        state: NonFinalState<&Self>,
        _text: &wstr,
        _idx: usize,
        c: char,
    ) -> State<Self> {
        let char_class = if c == '\n' {
            WordCharClass::Newline
        } else if is_blank(c) {
            WordCharClass::Blank
        } else {
            WordCharClass::Word
        };
        next_word_movement_state(direction, state, char_class)
    }
}
impl WordMovementState for BigWordMovementState {
    fn from_last_char_class(last_char_class: WordCharClass) -> Self {
        Self { last_char_class }
    }
    fn last_char_class(&self) -> WordCharClass {
        self.last_char_class
    }
}

fn next_word_movement_state<WMS: WordMovementState>(
    direction: MoveWordDir,
    state: NonFinalState<&WMS>,
    char_class: WordCharClass,
) -> State<WMS> {
    let last_char_class_state = match state {
        NonFinalState::Initial => None,
        NonFinalState::Live(wms) => Some(wms.last_char_class()),
    };
    if consume_char_word_movement(direction, last_char_class_state, char_class).is_break() {
        return State::Final;
    }
    State::Live(WMS::from_last_char_class(char_class))
}

fn consume_char_word_movement(
    direction: MoveWordDir,
    last_char_class: Option<WordCharClass>,
    cur_class: WordCharClass,
) -> ControlFlow<()> {
    enum Transition {
        Blank,
        Newline,
        SameClass,
        DifferentClass,
    }
    use Transition as T;
    let transition = if cur_class == WordCharClass::Blank {
        T::Blank
    } else if cur_class == WordCharClass::Newline {
        T::Newline
    } else if last_char_class == Some(cur_class) {
        T::SameClass
    } else {
        T::DifferentClass
    };

    let Some(last_char_class) = last_char_class else {
        return ControlFlow::Continue(());
    };
    if match direction {
        MoveWordDir::Left => match last_char_class {
            WordCharClass::Blank => false,
            WordCharClass::Newline => matches!(transition, T::Newline),
            _ => matches!(transition, T::Blank | T::Newline | T::DifferentClass),
        },
        MoveWordDir::Right => match last_char_class {
            WordCharClass::Blank => {
                matches!(transition, T::SameClass | T::DifferentClass)
            }
            WordCharClass::Newline => {
                matches!(transition, T::Newline | T::SameClass | T::DifferentClass)
            }
            _ => matches!(transition, T::DifferentClass),
        },
    } {
        ControlFlow::Break(())
    } else {
        ControlFlow::Continue(())
    }
}

/// a path component contains either
/// - [space +] end word
/// - punc + end word
/// - space + end punc
enum PathComponentTransition {
    Blank,
    PathComponent,
    Punctuation,
}

fn path_component_state_transition(
    text: &wstr,
    idx: usize,
    c: char,
) -> ControlFlow<(), PathComponentTransition> {
    let escaped = is_backslashed(text, idx);
    if c == '\\' && !escaped {
        return ControlFlow::Break(());
    };

    use PathComponentTransition as T;
    ControlFlow::Continue(if is_blank(c) && !escaped {
        T::Blank
    } else if is_path_component_character(c) || (is_blank(c) && escaped) {
        T::PathComponent
    } else {
        T::Punctuation
    })
}

fn is_path_component_character(c: char) -> bool {
    tok_is_string_character(c, None) && !L!("/={,}'\":@#").as_char_slice().contains(&c)
}

#[derive(Clone, Copy)]
enum PathComponentForwardState {
    PathComponent,
    Punctuation,
    Blank,
}
impl HasNextState for PathComponentForwardState {
    fn next_state(
        _direction: MoveWordDir,
        state: NonFinalState<&Self>,
        text: &wstr,
        idx: usize,
        c: char,
    ) -> State<Self> {
        // Forward path component movement: skip current homogeneous component, stop at next.
        // Each component type (word, slash, punctuation, whitespace) is a unit.

        let ControlFlow::Continue(transition) = path_component_state_transition(text, idx, c)
        else {
            return match state {
                NFS::Initial => State::Initial,
                NFS::Live(s) => State::Live(*s),
            };
        };

        use {NonFinalState as NFS, PathComponentForwardState as S, PathComponentTransition as T};
        let final_state = State::Final;
        State::Live(match state {
            NFS::Initial | NFS::Live(S::PathComponent) => match transition {
                T::Blank => S::Blank,
                T::PathComponent => S::PathComponent,
                T::Punctuation => S::Punctuation,
            },
            NFS::Live(S::Punctuation) => match transition {
                T::Blank => S::Blank,
                T::PathComponent => return final_state,
                T::Punctuation => S::Punctuation,
            },
            NFS::Live(S::Blank) => match transition {
                T::Blank => S::Blank,
                T::PathComponent => return final_state,
                T::Punctuation => return final_state,
            },
        })
    }
}

#[derive(Clone, Copy)]
enum PathComponentBackwardState {
    Punctuation,
    PathComponent,
    Space,
    EndPathComponent,
    EndPunctuation,
}
impl HasNextState for PathComponentBackwardState {
    fn next_state(
        _direction: MoveWordDir,
        state: NonFinalState<&Self>,
        text: &wstr,
        idx: usize,
        c: char,
    ) -> State<Self> {
        use {NonFinalState as NFS, PathComponentBackwardState as S, PathComponentTransition as T};

        let ControlFlow::Continue(transition) = path_component_state_transition(text, idx, c)
        else {
            return match state {
                NFS::Initial => State::Initial,
                NFS::Live(s) => State::Live(*s),
            };
        };

        let final_state = State::Final;

        State::Live(match state {
            NFS::Initial => match transition {
                T::Blank => S::Space,
                T::PathComponent => S::PathComponent,
                T::Punctuation => S::Punctuation,
            },
            NFS::Live(ls) => match ls {
                S::Punctuation => match transition {
                    T::Blank => return final_state,
                    T::PathComponent => S::EndPathComponent,
                    T::Punctuation => S::Punctuation,
                },
                S::PathComponent => match transition {
                    T::Blank => return final_state,
                    T::PathComponent => S::EndPathComponent,
                    T::Punctuation => return final_state,
                },
                S::Space => match transition {
                    T::Blank => S::Space,
                    T::PathComponent => S::PathComponent,
                    T::Punctuation => S::EndPunctuation,
                },
                S::EndPathComponent => match transition {
                    T::Blank => return final_state,
                    T::PathComponent => S::EndPathComponent,
                    T::Punctuation => return final_state,
                },
                S::EndPunctuation => match transition {
                    T::Blank => return final_state,
                    T::PathComponent => return final_state,
                    T::Punctuation => S::EndPunctuation,
                },
            },
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
                let new_sm = || MoveWordStateMachine::new($style, direction);
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
        validate!(Left, PathComponents, r#"^a\  ^b\ c/^d"^e\\\ f"^g"#);
        validate!(Left, PathComponents, r#"^a\"^bc^"#);

        validate!(Right, PathComponents, "^/^foo/^bar/^baz/^");
        validate!(Right, PathComponents, "^echo ^--foo ^--bar^");
        validate!(Right, PathComponents, "^echo ^hi ^> ^/^dev/^null^");
        validate!(Right, PathComponents, "^echo ^/^foo/^bar{^aaa,^ccc}^bak/^");
        validate!(Right, PathComponents, "^echo ^bak ^///^");
        validate!(Right, PathComponents, "^aaa ^@ ^@^aaa^");
        validate!(Right, PathComponents, "^aa@@ ^aa@@^a^");

        // General punctuation tests
        validate!(Left, Punctuation, "^a ^bcd^");
        validate!(Left, Punctuation, "^ab ^cd^e");
        validate!(Left, Punctuation, "^ab ^c^de");
        validate!(Left, Punctuation, "^ab ^cde^");
        validate!(Right, Punctuation, "^a ^bcd^");
        validate!(Right, Punctuation, "a^b ^cde^");
        validate!(Right, Punctuation, "ab^ ^cde^");
        validate!(Right, Punctuation, "^ab ^cde^");

        validate!(Left, Punctuation, "^echo ^hello^_^world^.^txt^");
        validate!(Right, Punctuation, "^echo ^hello^_^world^.^txt^");

        validate!(Left, Punctuation, "^echo ^foo^__^foo^_^foo^//    ^");
        validate!(Right, Punctuation, "^echo ^foo^__^foo^_^foo^//^");

        validate!(Right, Punctuation, "^ab^&^cd ^& ^e ^f^&^");
        validate!(Left, Punctuation, "^ab^&^cd ^& ^e ^f^&^");

        // General Whiltespace tests
        validate!(Left, Whitespace, "^a ^bcd^");
        validate!(Left, Whitespace, "^ab ^cd^e");
        validate!(Left, Whitespace, "^ab ^c^de");
        validate!(Left, Whitespace, "^ab ^cde^");
        validate!(Right, Whitespace, "^a ^bcd^");
        validate!(Right, Whitespace, "a^b ^cde^");
        validate!(Right, Whitespace, "ab^ ^cde^");
        validate!(Right, Whitespace, "^ab ^cde^");

        // Newline-related tests
        validate!(Right, Punctuation, "^a \n ^bcd^");
        validate!(Left, Punctuation, "^a \n ^bcd^");
        validate!(Right, Whitespace, "^a \n ^bcd^");
        validate!(Left, Whitespace, "^a \n ^bcd^");

        validate!(Right, Punctuation, "^a\n^\n^b^-^cd^");
        validate!(Left, Punctuation, "^a\n^\n^b^_^cd^");
        validate!(Right, Whitespace, "^a\n^\n^b_cd^");
        validate!(Left, Whitespace, "^a\n^\n^b_cd^");

        validate!(Right, Punctuation, "^a \n  \n \n^\n ^bcd^");
        validate!(Left, Punctuation, "^a \n  \n \n^\n ^bcd^");
        validate!(Right, Whitespace, "^a \n  \n \n^\n ^bcd^");
        validate!(Left, Whitespace, "^a \n  \n \n^\n ^bcd^");

        // Unicode-related tests
        validate!(Right, Punctuation, "^hello ^中^@^文^あいう^漢字^");
        validate!(Left, Punctuation, "^hello ^中^@^文^あいう^漢字^");
        validate!(Right, Whitespace, "^hello ^中文あいう漢字^");
        validate!(Left, Whitespace, "^hello ^中文あいう漢字^");

        validate!(Right, Punctuation, "^café ^naïve^");
        validate!(Left, Punctuation, "^café ^naïve^");
    }
}
