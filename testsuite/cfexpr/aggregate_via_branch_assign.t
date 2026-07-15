//@ expect val 42
struct P { u32 v }
fn main() i32 {
    P p = {.v = 0}
    i32 c = 1
    if c > 0 { p = {.v = 42} } else { p = {.v = 0} }
    return (i32) p.v
}
