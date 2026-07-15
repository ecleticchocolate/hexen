//@ expect val 50
struct I{f64 v} struct O{I i f64 k} fn main()i32{O o={.i={.v=2},.k=3} return (i32)((o.i.v+o.k)*10.0)}
