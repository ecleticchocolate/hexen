//@ expect val 42
struct P { u32 v }
fn six(P a, P b, P c, P d, P e, P f) u32 { return a.v+b.v+c.v+d.v+e.v+f.v }
fn main() i32 { return (i32) six({.v=1},{.v=2},{.v=3},{.v=4},{.v=5},{.v=27}) }
