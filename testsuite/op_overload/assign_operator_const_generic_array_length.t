//@ expect val 15
// __assign can declare its OWN const-generic value parameter (u32 N) for the
// array-literal RHS's length, inferred per call site from the literal's
// actual element count -- not hardcoded to one size. `Wrapper w = {1,2,3}`
// and `Wrapper w = {1,2,3,4,5}` each monomorphize __assign for their own N,
// the same const-generic inference Vec[T, u32 N]-style structs already get,
// now composing with the declaration-initializer sugar (parser.c's
// make_decl_stmt) and operator-overload dispatch together.
struct Wrapper { i32 v }
impl Wrapper {
    fn __assign[u32 N](i32[N] arr) void {
        u32 sum = 0
        for u32 i = 0 to N {
            sum = sum + arr[i]
        }
        self.v = sum
    }
}
fn main() i32 {
    Wrapper w = { 1, 2, 3, 4, 5 }
    return w.v
}
