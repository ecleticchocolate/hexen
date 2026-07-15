//@ expect err without a return
// `fn f() u32 { }` used to compile clean and return whatever was left in the return
// register (observed: 33, then -814499984 on the next run). Silent wrongness -- §1.
fn f() u32 { }
fn main() u32 { return f() }
