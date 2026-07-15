//@ expect err cannot use a bare type as an operand of an operator
// Bare `T == X` as an expression is removed, not just unimplemented. It used
// to fold to a bool literal in Typecheck_Tree's default case, but that fold
// was one arm of a two-arm special case a chained `else if` could route
// around -- an unfolded AST_TYPE_EXPR (no codegen case at all) reaching
// codegen and corrupting memory. The sanctioned way to branch on a type is
// `match T { u32 {..} else {..} }` (specs.md §17.4), which lowers straight to
// reflect_pattern/reflect_scrutinee and never produces AST_EQ over
// AST_TYPE_EXPR at all -- so it's unaffected by this removal.
struct Box[T] { T val }
impl Box[T] {
    fn is_i32() bool { return T == i32 }
}
fn main() i32 {
    Box[i32] b
    return (i32)b.is_i32()
}
