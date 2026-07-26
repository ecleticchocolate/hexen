//@ expect err closure cannot capture an owning value by value
//@ use std/option.t std/iterator.t std/vector.t std/closure.t
// A guard built from "return an identifier that does not exist" is disabled by
// declaring that identifier. This file declares a global named after the old
// sentinel: with the trick it compiled and double-freed at runtime; with a real
// static_assert the check still fires.
i32 CLOSURE_CANNOT_CAPTURE_OWNING_VALUE_PASS_A_POINTER_INSTEAD = 0
fn take[T](T... a) u32 { unpack {v} = a  return v.len() }
fn main() i32 {
    Vector[u8] d = Vector[u8].create()
    d.push(1)
    return (i32)closure(take, d)()
}
