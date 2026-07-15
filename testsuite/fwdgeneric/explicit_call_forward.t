//@ expect val 7
// An explicit generic call whose callee is declared BELOW it. Requires pass 0b
// (Parse_Signatures) to have recorded b's param_kinds, since the bracket list is
// parsed kind-driven. Before that pass existed, `b[T]` silently became an array
// index and died later with "indexing a non-array, non-pointer".
fn a[T](i32 x) i32 { return b[T](x) }
fn b[T](i32 x) i32 { return x + 2 }
fn main() i32 { return a[i32](5) }
