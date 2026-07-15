//@ expect val 40
struct P { u32 a  u32 b }
fn mk() P { return {.a = 40, .b = 2} }
fn main() i32 { return (i32) mk().a }
