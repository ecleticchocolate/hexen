//@ expect stdout
//@ | direct=1 forwarded=1
//@ | generic accepts both: 3 2
// A capability query must agree with what a CALL actually does.
//
// `super` forwarding is a call-site rewrite -- `o.len()` becomes
// `View_len(&o.v)` -- so no `Owned_len` symbol is ever created. Looking the
// method up by mangled name therefore answered "no" for a type whose calls
// plainly work: o.len() compiled and ran, while `match S { impl { fn len() } }`
// took the else arm for the same type.
//
// This is what lets ONE generic function accept both a borrowed view and an
// owned buffer with no conversion at the call site -- the pattern std/string.t
// needs so an API can take Str and be handed a String.
extern fn printf(u8* fmt, ...) i32
struct View { u8* data  u32 size }
impl View { fn len() u32 { return self.size } }
struct Owned { super View v  u32 capacity }
fn has_len[S]() u32 {
    match S {
        impl { fn len() u32 } { return 1 }
        else { return 0 }
    }
}
fn any_len[S](S s) u32 {
    match S {
        impl { fn len() u32 } { return s.len() }
        else { return 0 }
    }
}
fn main() i32 {
    printf("direct=%d forwarded=%d\n", (i32)has_len[View](), (i32)has_len[Owned]())
    View v
    v.data = "abc"  v.size = 3
    Owned o
    o.data = "hi"  o.size = 2
    printf("generic accepts both: %d %d\n", (i32)any_len(v), (i32)any_len(o))
    return 0
}
