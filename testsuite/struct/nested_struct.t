//@ expect val 15
struct Inner { i32 v }
struct Outer { Inner in  i32 k }
fn main() i32 { Outer o = {.in = {.v = 9}, .k = 6}; return o.in.v + o.k }
