//@ use std/option.t std/iterator.t std/vector.t std/closure.t
//@ expect stdout
//@ | captured=2
//@ | source=2
// Capturing an owning value: the bundle is a byte copy, so it ALIASES the
// Vector's buffer rather than duplicating it. Capture a POINTER, not the value
// -- then the closure holds a borrowed view, the original stays the only owner,
// and there is exactly one destructor call at the end of main.
//
// Capturing `data` itself instead of `&data` compiles and then double-frees:
// the Fn temporary and the local both destroy the same buffer. That is the
// documented cost of byte-copy capture, not a closure-specific defect.
extern fn printf(u8* fmt, ...) i32
fn take_len[T](T... args) u32 {
    unpack {p} = args
    return p.len()
}
fn main() i32 {
    Vector[u8] data = Vector[u8].create()
    data.push(1)  data.push(2)
    printf("captured=%d\n", (i32)closure(take_len, &data)())
    printf("source=%d\n", (i32)data.len())
    return 0
}
