//@ expect val 42
// Regression: an impl method's OWN generic type parameter (`fn get[T]() T`)
// stopped being recognized as a type the moment parsing moved from the
// signature into the body -- `(T)self.v` inside the body errored
// "undeclared identifier T", even though T resolved correctly in the
// signature (params/return type) just parsed a moment before. Cause:
// parser.c's impl-method parsing restored s_type_params/s_type_param_count
// back to the enclosing impl-block/struct scope immediately after the
// signature, BEFORE parsing the body -- one statement too early. Fixed by
// moving that restore to after the body is parsed.
struct Box { i32 v }
impl Box {
    fn get[T]() T { return (T)self.v }
}
fn main() i32 {
    Box b = { .v = 42 }
    i32 x = b.get[i32]()
    return x
}
