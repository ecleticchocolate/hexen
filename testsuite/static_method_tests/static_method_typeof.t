//@ expect val 3
// `typeof(x).method()`: a VALUE's static type queried via typeof, then a
// static method resolved on that type. Same AST_TYPE_EXPR convergence as
// the literal-name and generic-param cases -- typeof(x) evaporates to a
// pure compile-time type before the call-rewrite ever runs; nothing about
// `p` (the value typeof was applied to) is passed through or retained.
struct Point { i32 x  i32 y }
impl Point {
    static fn origin() Point { return {.x = 0, .y = 0} }
}
fn main() i32 {
    Point p = {.x = 9, .y = 9}
    typeof(p) q = typeof(p).origin()
    return q.x + q.y + 3
}
