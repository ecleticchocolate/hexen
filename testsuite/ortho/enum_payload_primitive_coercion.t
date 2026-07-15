//@ expect val 1
enum Option[T] { T Some  None }
fn main() i32 {
    // bool -> u32 is an allowed implicit primitive coercion language-wide
    // (like `u32 x = true`), so a bool payload into Option[u32] coerces too.
    // The enum path is consistent with plain assignment, not stricter.
    Option[u32] a = .Some{true}
    match a { .Some{v} { return (i32)v } .None { return 0 } }
}
