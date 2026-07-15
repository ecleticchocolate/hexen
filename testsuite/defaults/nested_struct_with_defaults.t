//@ expect val 99
struct Inner { i32 x = 7  i32 y = 8 }
struct Outer { Inner a  i32 b = 99 }
fn main() i32 {
    Outer o = {}
    return o.a.x + o.b
}
