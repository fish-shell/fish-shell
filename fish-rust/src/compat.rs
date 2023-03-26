#[allow(non_snake_case)]
pub fn MB_CUR_MAX() -> usize {
    unsafe { C_MB_CUR_MAX() }
}

extern "C" {
    fn C_MB_CUR_MAX() -> usize;
}
