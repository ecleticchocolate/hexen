//@ expect val 6
struct Inner { u32 a  u32 b }
struct Outer { Inner in  u32 c }
fn build() u32 {
    Outer o
    o.in = {.a = 1, .b = 2}
    o.c = 3
    return o.in.a + o.in.b + o.c
}
const u32 TOTAL = build()
fn main() i32 { return (i32)TOTAL }
