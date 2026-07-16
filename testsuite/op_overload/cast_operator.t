//@ expect val 0
// (T)x -> x.__cast[T](). A single generic __cast[T]() dispatches to two
// different target types from the SAME method, monomorphized per cast --
// the explicit type argument attaches via the same node->call.type_args
// mechanism an ordinary explicit generic call already uses.
struct Meters { i32 v }
impl Meters {
    fn __cast[T]() T { return (T)(self.v * 100) }
}
fn main() i32 {
    Meters m = { .v = 5 }
    f64 cm = (f64)m
    i32 icm = (i32)m
    if ((i32)cm != 500) { return 1 }
    if (icm != 500) { return 2 }
    return 0
}
