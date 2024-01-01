use autocxx::prelude::*;

// autocxx has been hacked up to know about this.
pub type wchar_t = u32;

include_cpp! {
    #include "common.h"
}
