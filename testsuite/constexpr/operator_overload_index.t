//@ expect val 0
// Same regression class as operator_overload_add_eq.t, for __index. Also
// exercises a placement bug: the ConstEval dispatch hook must run before
// AST_INDEX's own case (which lives in ConstEval_inner's FIRST switch), not
// merely before that switch's `default` -- __add/__eq have no case of their
// own there and fall through to `default` either way, so they could not have
// caught this, but __index's own case returns directly and would silently
// keep reading raw array bytes if the hook were placed too late.
struct Vec { i32 x  i32 y }
impl Vec {
    fn __index(i32 i) i32 { return 777 }
}
fn build() i32 {
    Vec v = { .x = 1, .y = 2 }
    return v[0]
}
const i32 R = build()
fn main() i32 { if (R != 777) { return 1 } return 0 }
