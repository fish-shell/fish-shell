use crate::ffi::wcstring_list_ffi_t;
use crate::input::*;
use crate::input_common::*;
use crate::parser::ParserRefFFI;
use crate::threads::CppCallback;
use crate::wchar::prelude::*;
use crate::wchar_ffi::AsWstr;
use crate::wchar_ffi::WCharToFFI;
use cxx::CxxWString;
pub use ffi::{CharInputStyle, ReadlineCmd};
use std::pin::Pin;

// Returns the code, or -1 on failure.
fn input_function_get_code_ffi(name: &CxxWString) -> i32 {
    if let Some(code) = input_function_get_code(name.as_wstr()) {
        code.repr as i32
    } else {
        -1
    }
}

fn char_event_from_readline_ffi(cmd: ReadlineCmd) -> Box<CharEvent> {
    Box::new(CharEvent::from_readline(cmd))
}

fn char_event_from_char_ffi(c: u8) -> Box<CharEvent> {
    Box::new(CharEvent::from_char(c.into()))
}

fn make_inputter_ffi(parser: &ParserRefFFI, in_fd: i32) -> Box<Inputter> {
    Box::new(Inputter::new(parser.0.clone(), in_fd))
}

fn make_input_event_queue_ffi(in_fd: i32) -> Box<InputEventQueue> {
    Box::new(InputEventQueue::new(in_fd))
}

fn input_terminfo_get_name_ffi(seq: &CxxWString, out: Pin<&mut CxxWString>) -> bool {
    let Some(name) = input_terminfo_get_name(seq.as_wstr()) else {
        return false;
    };
    out.push_chars(name.as_char_slice());
    true
}

impl Inputter {
    #[allow(clippy::boxed_local)]
    fn queue_char_ffi(&mut self, ch: Box<CharEvent>) {
        self.queue_char(*ch);
    }

    fn read_char_ffi(&mut self, command_handler: &cxx::SharedPtr<CppCallback>) -> Box<CharEvent> {
        let mut rust_handler = |cmds: &[WString]| {
            let ffi_cmds = cmds.to_ffi();
            command_handler.invoke_with_param(ffi_cmds.as_ref().unwrap() as *const _ as *const u8);
        };
        let mhandler = if !command_handler.is_null() {
            Some(&mut rust_handler as &mut CommandHandler)
        } else {
            None
        };
        Box::new(self.read_char(mhandler))
    }

    fn function_pop_arg_ffi(&mut self) -> u32 {
        self.function_pop_arg().into()
    }
}

impl CharEvent {
    fn get_char_ffi(&self) -> u32 {
        self.get_char().into()
    }

    fn get_input_style_ffi(&self) -> CharInputStyle {
        self.input_style
    }
}

impl InputEventQueue {
    // Returns Box<CharEvent>::into_raw(), or nullptr if None.
    fn readch_timed_esc_ffi(&mut self) -> *mut CharEvent {
        match self.readch_timed_esc() {
            Some(ch) => Box::into_raw(Box::new(ch)),
            None => std::ptr::null_mut(),
        }
    }
}

#[cxx::bridge]
mod ffi {
    /// Hackish: the input style, which describes how char events (only) are applied to the command
    /// line. Note this is set only after applying bindings; it is not set from readb().
    #[cxx_name = "char_input_style_t"]
    #[repr(u8)]
    #[derive(Debug, Copy, Clone)]
    pub enum CharInputStyle {
        // Insert characters normally.
        Normal,

        // Insert characters only if the cursor is not at the beginning. Otherwise, discard them.
        NotFirst,
    }

    #[cxx_name = "readline_cmd_t"]
    #[repr(u8)]
    #[derive(Debug, Copy, Clone, PartialEq, Eq)]
    pub enum ReadlineCmd {
        BeginningOfLine,
        EndOfLine,
        ForwardChar,
        BackwardChar,
        ForwardSingleChar,
        ForwardWord,
        BackwardWord,
        ForwardBigword,
        BackwardBigword,
        NextdOrForwardWord,
        PrevdOrBackwardWord,
        HistorySearchBackward,
        HistorySearchForward,
        HistoryPrefixSearchBackward,
        HistoryPrefixSearchForward,
        HistoryPager,
        HistoryPagerDelete,
        DeleteChar,
        BackwardDeleteChar,
        KillLine,
        Yank,
        YankPop,
        Complete,
        CompleteAndSearch,
        PagerToggleSearch,
        BeginningOfHistory,
        EndOfHistory,
        BackwardKillLine,
        KillWholeLine,
        KillInnerLine,
        KillWord,
        KillBigword,
        BackwardKillWord,
        BackwardKillPathComponent,
        BackwardKillBigword,
        HistoryTokenSearchBackward,
        HistoryTokenSearchForward,
        SelfInsert,
        SelfInsertNotFirst,
        TransposeChars,
        TransposeWords,
        UpcaseWord,
        DowncaseWord,
        CapitalizeWord,
        TogglecaseChar,
        TogglecaseSelection,
        Execute,
        BeginningOfBuffer,
        EndOfBuffer,
        RepaintMode,
        Repaint,
        ForceRepaint,
        UpLine,
        DownLine,
        SuppressAutosuggestion,
        AcceptAutosuggestion,
        BeginSelection,
        SwapSelectionStartStop,
        EndSelection,
        KillSelection,
        InsertLineUnder,
        InsertLineOver,
        ForwardJump,
        BackwardJump,
        ForwardJumpTill,
        BackwardJumpTill,
        FuncAnd,
        FuncOr,
        ExpandAbbr,
        DeleteOrExit,
        Exit,
        CancelCommandline,
        Cancel,
        Undo,
        Redo,
        BeginUndoGroup,
        EndUndoGroup,
        RepeatJump,
        DisableMouseTracking,
        // ncurses uses the obvious name
        ClearScreenAndRepaint,
        // NOTE: This one has to be last.
        ReverseRepeatJump,
    }

    extern "C++" {
        include!("parser.h");
        include!("reader.h");
        include!("callback.h");
        type wcstring_list_ffi_t = super::wcstring_list_ffi_t;
        type ParserRef = crate::parser::ParserRefFFI;

        #[rust_name = "CppCallback"]
        type callback_t = crate::threads::CppCallback;
    }

    extern "Rust" {
        fn init_input();

        #[cxx_name = "input_function_get_code"]
        fn input_function_get_code_ffi(name: &CxxWString) -> i32;

        #[cxx_name = "input_terminfo_get_name"]
        fn input_terminfo_get_name_ffi(seq: &CxxWString, out: Pin<&mut CxxWString>) -> bool;
    }

    extern "Rust" {
        type CharEvent;

        #[cxx_name = "char_event_from_readline"]
        fn char_event_from_readline_ffi(cmd: ReadlineCmd) -> Box<CharEvent>;

        #[cxx_name = "char_event_from_char"]
        fn char_event_from_char_ffi(c: u8) -> Box<CharEvent>;

        fn is_char(&self) -> bool;
        fn is_readline(&self) -> bool;
        fn is_check_exit(&self) -> bool;
        fn is_eof(&self) -> bool;

        #[cxx_name = "get_char"]
        fn get_char_ffi(&self) -> u32;

        #[cxx_name = "get_input_style"]
        fn get_input_style_ffi(&self) -> CharInputStyle;

        fn get_readline(&self) -> ReadlineCmd;
    }

    extern "Rust" {
        type Inputter;

        #[cxx_name = "make_inputter"]
        fn make_inputter_ffi(parser: &ParserRef, in_fd: i32) -> Box<Inputter>;

        #[cxx_name = "queue_char"]
        fn queue_char_ffi(&mut self, ch: Box<CharEvent>);

        fn queue_readline(&mut self, cmd: ReadlineCmd);

        #[cxx_name = "read_char"]
        fn read_char_ffi(&mut self, command_handler: &SharedPtr<CppCallback>) -> Box<CharEvent>;

        fn function_set_status(&mut self, status: bool);

        #[cxx_name = "function_pop_arg"]
        fn function_pop_arg_ffi(&mut self) -> u32;
    }

    extern "Rust" {
        type InputEventQueue;

        #[cxx_name = "make_input_event_queue"]
        fn make_input_event_queue_ffi(in_fd: i32) -> Box<InputEventQueue>;

        #[cxx_name = "readch_timed_esc"]
        fn readch_timed_esc_ffi(&mut self) -> *mut CharEvent;
    }
}
