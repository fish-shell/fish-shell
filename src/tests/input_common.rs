use crate::input_common::{CharEvent, InputEventQueue, InputEventQueuer, ReadlineCmd};

#[test]
fn test_push_front_back() {
    let mut queue = InputEventQueue::new(0);
    queue.push_front(CharEvent::from_char('a'));
    queue.push_front(CharEvent::from_char('b'));
    queue.push_back(CharEvent::from_char('c'));
    queue.push_back(CharEvent::from_char('d'));
    assert_eq!(queue.try_pop().unwrap().get_char(), 'b');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'a');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'c');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'd');
    assert!(queue.try_pop().is_none());
}

#[test]
fn test_promote_interruptions_to_front() {
    let mut queue = InputEventQueue::new(0);
    queue.push_back(CharEvent::from_char('a'));
    queue.push_back(CharEvent::from_char('b'));
    queue.push_back(CharEvent::from_readline(ReadlineCmd::Undo));
    queue.push_back(CharEvent::from_readline(ReadlineCmd::Redo));
    queue.push_back(CharEvent::from_char('c'));
    queue.push_back(CharEvent::from_char('d'));
    queue.promote_interruptions_to_front();

    assert_eq!(queue.try_pop().unwrap().get_readline(), ReadlineCmd::Undo);
    assert_eq!(queue.try_pop().unwrap().get_readline(), ReadlineCmd::Redo);
    assert_eq!(queue.try_pop().unwrap().get_char(), 'a');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'b');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'c');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'd');
    assert!(!queue.has_lookahead());

    queue.push_back(CharEvent::from_char('e'));
    queue.promote_interruptions_to_front();
    assert_eq!(queue.try_pop().unwrap().get_char(), 'e');
    assert!(!queue.has_lookahead());
}

#[test]
fn test_insert_front() {
    let mut queue = InputEventQueue::new(0);
    queue.push_back(CharEvent::from_char('a'));
    queue.push_back(CharEvent::from_char('b'));

    let events = vec![
        CharEvent::from_char('A'),
        CharEvent::from_char('B'),
        CharEvent::from_char('C'),
    ];
    queue.insert_front(events);
    assert_eq!(queue.try_pop().unwrap().get_char(), 'A');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'B');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'C');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'a');
    assert_eq!(queue.try_pop().unwrap().get_char(), 'b');
}
