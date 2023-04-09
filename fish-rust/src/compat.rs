#[allow(non_snake_case)]
pub fn MB_CUR_MAX() -> usize {
    unsafe { C_MB_CUR_MAX() }
}

pub fn cur_term() -> bool {
    unsafe { has_cur_term() }
}

extern "C" {
    fn C_MB_CUR_MAX() -> usize;
    fn has_cur_term() -> bool;
}
