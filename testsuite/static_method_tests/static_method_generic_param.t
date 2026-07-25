//@ expect val 5
// `T.method()` inside a generic function: a bare generic type-param
// resolves to the same AST_TYPE_EXPR node a literal struct name does
// once monomorphized, so the identical resolution branch handles it
// with no special-casing. Confirms two distinct instantiations each
// dispatch to their OWN type's static method, not a shared/confused one.
struct Point { i32 x  i32 y }
impl Point {
    static fn origin() Point { return {.x = 0, .y = 0} }
}
struct Vec3 { i32 x  i32 y  i32 z }
impl Vec3 {
    static fn origin() Vec3 { return {.x = 0, .y = 0, .z = 0} }
}
fn make_origin[T]() T {
    return T.origin()
}
fn main() i32 {
    Point p = make_origin[Point]()
    Vec3 v = make_origin[Vec3]()
    p.x = 2
    v.z = 3
    return p.x + p.y + v.x + v.y + v.z
}
