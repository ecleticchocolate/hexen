//@ expect val 7
// Regression: a+b on a generic struct's overloaded operator used to reach
// codegen typed as the unsubstituted Box[T] instead of Box[i32] (a type
// mismatch the caller rejected), because infer_generic ran on the raw AST_ADD
// node before Type_Infer's lazy rewrite to AST_CALL ever happened -- so the
// self_type_args substitution never had a chance to apply. Calling __add BY
// NAME (a.__add(b)) already worked; only the `+` sugar path was broken.
struct Box[T] { T v }
impl Box[T] {
    fn __add(Box[T] other) Box[T] {
        return { .v = self.v + other.v }
    }
}
fn main() i32 {
    Box[i32] a = { .v = 3 }
    Box[i32] b = { .v = 4 }
    Box[i32] c = a + b
    return c.v
}
