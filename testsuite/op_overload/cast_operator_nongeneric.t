//@ expect val 0
// A non-generic __cast() -> ONE fixed return type -- must only apply when
// the cast's own target type matches that fixed return type. Casting to a
// mismatched type must NOT silently "dispatch" (it used to: attaching an
// unused type_args entry to a call with no type parameter to substitute it
// into was a silent no-op, so `(i32)m` wrongly ran __cast() [-> f64] and
// reinterpreted the f64 bits as i32 instead of falling back / erroring).
struct Meters { i32 v }
impl Meters {
    fn __cast() f64 { return (f64)self.v * 100.0 }
}
fn main() i32 {
    Meters m = { .v = 5 }
    f64 cm = (f64)m
    if ((i32)cm != 500) { return 1 }
    return 0
}
