//! History functions, part of the user interface.

use crate::env::Environment;
use crate::wchar::{wstr, WString};
use std::collections::HashMap;
use std::rc::Rc;

pub struct History {}

impl History {
    pub fn with_name(name: &wstr) -> Rc<History> {
        todo!()
    }
    pub fn size(&self) -> usize {
        todo!()
    }
    pub fn get_history(&self, out: &mut Vec<WString>) {
        todo!()
    }
    pub fn items_at_indexes(&self, idxs: &[i64]) -> HashMap<i64, WString> {
        todo!()
    }
}

pub fn history_session_id(vars: &dyn Environment) -> WString {
    todo!()
}
