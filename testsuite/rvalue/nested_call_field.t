//@ expect val 42
struct I { u32 v }
struct O { I i }
fn mk() O { return {.i = {.v = 42}} }
fn main() i32 { return (i32) mk().i.v }
