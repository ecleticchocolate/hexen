//@ expect val 30
struct V{f64 x}
V g={.x=3}
fn main()i32{return (i32)(g.x*10.0)}
