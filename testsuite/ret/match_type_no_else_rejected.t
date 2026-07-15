//@ expect err without a return
// A `match T` whose arms miss and which has no `else` falls off the end. A type-match
// cannot be exhaustive over the infinite set of types, so `else` is the only way to
// close it (§23) -- and omitting it must not silently yield a junk value.
fn f[T]() u32 {
    match T { u8 { return 1 } }
}
fn main() u32 { return f[u32]() }
