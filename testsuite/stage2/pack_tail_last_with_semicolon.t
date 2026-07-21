//@ expect err last field
// The pack-tail-must-be-last check ran in an `else` after the separator branch,
// so it only fired for the old `A... a  B b` spelling and silently accepted the
// `;`-separated form.
fn p[T]() i32 { match T { struct { A...; B } { return 1 } else { return 2 } } return -1 }
fn main() i32 { return p[struct{i32 x}]() }
