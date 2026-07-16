//@ expect val 9
// Regression guard for the fix above: ordinary field-then-index access
// (b.arr[i]) must still parse as indexing, not get mistaken for a
// type-argument list, even though both start with `.name[`.
struct Box { i32[3] arr }
fn main() i32 {
    Box b = { .arr = { 5, 9, 13 } }
    return b.arr[1]
}
