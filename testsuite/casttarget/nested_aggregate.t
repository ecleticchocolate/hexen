//@ expect val 42
struct Inner { u32 v }
struct Outer { Inner i  u32 k }
fn get(Outer o) u32 { return o.i.v + o.k }
fn main() i32 { return (i32) get((Outer){.i = {.v = 5}, .k = 37}) }
