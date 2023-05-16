use bitflags::bitflags;

bitflags! {
    pub struct ColorSupport: u8 {
        const NONE = 0;
        const TERM_256COLOR = 1<<0;
        const TERM_24BIT = 1<<1;
    }
}

pub fn output_set_color_support(value: ColorSupport) {
    extern "C" {
        pub fn output_set_color_support(value: libc::c_int);
    }

    unsafe {
        output_set_color_support(value.bits() as i32);
    }
}
