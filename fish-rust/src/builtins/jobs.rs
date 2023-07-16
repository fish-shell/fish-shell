//! Implementation of the jobs builtin.

use libc::c_int;
use crate::ffi::parser_t;
use super::shared::{builtin_missing_argument, io_streams_t, STATUS_CMD_OK, STATUS_INVALID_ARGS};
use crate::wchar::{encode_byte_to_char, wstr, WString, L};
use crate::wgetopt::{wgetopter_t, woption};

pub fn jobs(
    parser: &mut parser_t,
    streams: &mut io_streams_t,
    args: &mut [&wstr],
) -> Option<c_int> {
    Some(0)
}
