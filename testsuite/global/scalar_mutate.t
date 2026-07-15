//@ expect val 7
u32 g = 5
fn bump() { g = g + 1 }
fn main() i32 { bump() bump() return (i32) g }
