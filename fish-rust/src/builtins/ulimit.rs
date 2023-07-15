use super::prelude::*;

pub fn ulimit(parser: &Parser, streams: &mut IoStreams<'_>, args: &mut [WString]) -> Option<c_int> {
    todo!()
}

/// Struct describing a resource limit.
struct Resource {
    resource: c_int,     // resource ID
    desc: &'static wstr, // description of resource
    switch_char: char,   // switch used on commandline to specify resource
    multiplier: c_int,   // the implicit multiplier used when setting getting values
}

/// Array of resource_t structs, describing all known resource types.
const resource_arr: &[Resource] = &[];
