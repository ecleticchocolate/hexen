//@ use std/option.t std/iterator.t std/vector.t std/closure.t
//@ expect err closure cannot capture an owning value by value
// Capturing an owning value BY VALUE is a compile error, not a runtime double
// free. The guard is written in the language itself -- peel the capture bundle,
// ask the Owning capability, static_assert on the answer.
//
// It used to fake the assert by naming an identifier that does not exist. That
// worked until someone declared that name: a global with the sentinel's name
// silently disabled the check and the double free came back (verified). An
// assert cannot be shadowed.
fn take_len[T](T... args) u32 {
    unpack {v} = args
    return v.len()
}
fn main() i32 {
    Vector[u8] data = Vector[u8].create()
    data.push(1)
    return (i32)closure(take_len, data)()
}
