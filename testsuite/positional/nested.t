//@ expect val 42
struct Inner { u32 v }
struct Outer { Inner i  u32 k }
fn main() i32 {
    Outer o = {{5}, 37}
    return (i32)(o.i.v + o.k)
}
