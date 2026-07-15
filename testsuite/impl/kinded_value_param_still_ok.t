//@ expect val 10
// The control for the specialization check: a slot is a PARAMETER DECLARATION iff it
// ends in a name that is not already a type. `u32[4] W` ends in `W`, so it is a kinded
// value param, not a specialization -- a naive "does this slot contain a type?" check
// rejected this and broke 25 tests.
struct Tbl[T, u32[4] W] { T v }
impl Tbl[T, u32[4] W] { fn tot() u32 { return W[0] + W[1] + W[2] + W[3] } }
fn main() u32 {
    Tbl[u32, {1, 2, 3, 4}] t = {.v = 0}
    return t.tot()
}
