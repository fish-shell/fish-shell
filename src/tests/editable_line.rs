use crate::{
    editable_line::{Edit, EditableLine},
    wchar::prelude::*,
};

#[test]
fn test_undo() {
    let mut line = EditableLine::default();

    let insert = |line: &EditableLine| line.position()..line.position();

    assert!(!line.undo()); // nothing to undo
    assert!(line.text().is_empty());
    assert_eq!(line.position(), 0);
    line.push_edit(Edit::new(0..0, L!("a b c").to_owned()), true);
    assert_eq!(line.text(), L!("a b c").to_owned());
    assert_eq!(line.position(), 5);
    line.set_position(2);
    line.push_edit(Edit::new(2..3, L!("B").to_owned()), true); // replacement right of cursor
    assert_eq!(line.text(), L!("a B c").to_owned());
    line.undo();
    assert_eq!(line.text(), L!("a b c").to_owned());
    assert_eq!(line.position(), 2);
    line.redo();
    assert_eq!(line.text(), L!("a B c").to_owned());
    assert_eq!(line.position(), 3);

    assert!(!line.redo()); // nothing to redo

    line.push_edit(Edit::new(0..2, L!("").to_owned()), true); // deletion left of cursor
    assert_eq!(line.text(), L!("B c").to_owned());
    assert_eq!(line.position(), 1);
    line.undo();
    assert_eq!(line.text(), L!("a B c").to_owned());
    assert_eq!(line.position(), 3);
    line.redo();
    assert_eq!(line.text(), L!("B c").to_owned());
    assert_eq!(line.position(), 1);

    line.push_edit(Edit::new(0..line.len(), L!("a b c").to_owned()), true); // replacement left and right of cursor
    assert_eq!(line.text(), L!("a b c").to_owned());
    assert_eq!(line.position(), 5);

    // Undo coalesced edits
    line.push_edit(Edit::new(0..line.len(), L!("").to_owned()), false);
    line.push_edit(Edit::new(insert(&line), L!("a").to_owned()), true);
    line.push_edit(Edit::new(insert(&line), L!("b").to_owned()), true);
    line.push_edit(Edit::new(insert(&line), L!("c").to_owned()), true);
    line.push_edit(Edit::new(insert(&line), L!(" ").to_owned()), true);
    line.undo();
    line.undo();
    line.redo();
    assert_eq!(line.text(), L!("abc").to_owned());
    // This removes the space insertion from the history, but does not coalesce with the first edit.
    line.push_edit(Edit::new(insert(&line), L!("d").to_owned()), true);
    line.push_edit(Edit::new(insert(&line), L!("e").to_owned()), true);
    assert_eq!(line.text(), L!("abcde").to_owned());
    line.undo();
    assert_eq!(line.text(), L!("abc").to_owned());
}

#[test]
fn test_undo_group() {
    let mut line = EditableLine::default();
    line.begin_edit_group();
    line.push_edit(Edit::new(0..0, L!("a").to_owned()), true);
    line.end_edit_group();
    line.begin_edit_group();
    line.push_edit(Edit::new(1..1, L!("b").to_owned()), true);
    line.end_edit_group();
    line.undo();
    assert_eq!(line.text(), "a");
}
