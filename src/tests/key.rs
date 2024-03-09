use crate::key::{self, control, parse_keys, Chord};
use crate::wchar::prelude::*;

#[test]
fn test_parse_key() {
    assert_eq!(parse_keys(L!("[esc]")), Ok(vec![Chord::from(key::Escape)]));
    assert_eq!(parse_keys(L!("\x1b")), Ok(vec![Chord::from(key::Escape)]));
    assert_eq!(parse_keys(L!("[c-a]")), Ok(vec![control('a')]));
    assert_eq!(parse_keys(L!("\x01")), Ok(vec![control('a')]));
}
