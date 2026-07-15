//@ expect val 33
fn a(u32 x) u32 { return x + 1 }
fn b(u32 x) u32 { return x + 2 }
fn pair() (fn(u32) u32)[2] {
    (fn(u32) u32)[2] r
    r[0] = a
    r[1] = b
    return r
}
fn main() i32 {
    (fn(u32) u32)[2] fns = pair()
    return (i32)(fns[0](10) + fns[1](20))
}
