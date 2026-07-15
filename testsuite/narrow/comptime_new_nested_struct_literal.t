//@ expect val 3
struct Inner { i32 a  i32 b }
struct Outer { Inner in }
fn build() i32 {
    Outer* o = new Outer{.in = {.a = 1, .b = 2}}
    return o.in.a + o.in.b
}
const i32 R = build()
fn main() i32 { return R }
