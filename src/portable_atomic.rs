cfg_if::cfg_if! {
    if #[cfg(target_has_atomic = "64")] {
        pub use std::sync::atomic::AtomicU64;
    } else {
        pub use portable_atomic::AtomicU64;
    }
}
