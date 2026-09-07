//! Small helpers over raylib's C API.
//!
//! `measure_text` is only exposed as a method on the window handle, but text is
//! laid out deep inside the draw code where the handle is not in scope. Calling
//! the underlying function directly avoids threading the handle through every
//! drawing signature.

use std::ffi::CString;

pub fn measure_text(text: &str, font_size: i32) -> i32 {
    match CString::new(text) {
        Ok(c) => unsafe { raylib::ffi::MeasureText(c.as_ptr(), font_size) },
        // A string with an interior NUL cannot be measured; approximate rather
        // than panicking in the middle of a frame.
        Err(_) => text.len() as i32 * font_size / 2,
    }
}
