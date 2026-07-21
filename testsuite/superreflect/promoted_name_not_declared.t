//@ expect val -1
struct Base { u32 tag }
struct Derived { super Base info  u32 extra }
fn main() i32 { match Derived { struct { A tag; Rest... } { return 1 } else { return -1 } } }
