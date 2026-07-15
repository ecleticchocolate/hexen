//@ expect val 20
fn first(u32[3] a) u32 { return a[1] }
fn main() i32 { return (i32) first((u32[3]){10, 20, 30}) }
