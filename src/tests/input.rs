use crate::input::{input_mappings, Inputter, DEFAULT_BIND_MODE};
use crate::input_common::{CharEvent, ReadlineCmd};
use crate::parser::Parser;
use crate::tests::prelude::*;
use crate::wchar::prelude::*;
use std::sync::Arc;

#[test]
#[serial]
fn test_input() {
    test_init();
    use crate::env::EnvStack;
    let parser = Parser::new(Arc::pin(EnvStack::new()), false);
    let mut input = Inputter::new(parser, libc::STDIN_FILENO);
    // Ensure sequences are order independent. Here we add two bindings where the first is a prefix
    // of the second, and then emit the second key list. The second binding should be invoked, not
    // the first!
    let prefix_binding = WString::from_str("qqqqqqqa");
    let desired_binding = prefix_binding.clone() + "a";

    let default_mode = || DEFAULT_BIND_MODE.to_owned();

    {
        let mut input_mapping = input_mappings();
        input_mapping.add1(
            prefix_binding,
            WString::from_str("up-line"),
            default_mode(),
            None,
            true,
        );
        input_mapping.add1(
            desired_binding.clone(),
            WString::from_str("down-line"),
            default_mode(),
            None,
            true,
        );
    }

    // Push the desired binding to the queue.
    for c in desired_binding.chars() {
        input.queue_char(CharEvent::from_char(c));
    }

    // Now test.
    let evt = input.read_char(None);
    if !evt.is_readline() {
        panic!("Event is not a readline");
    } else if evt.get_readline() != ReadlineCmd::DownLine {
        panic!("Expected to read char down_line");
    }
}
