//@ expect val 110
fn f1(u32 x) u32 { return x + 100 }
fn f2(u32 x) u32 { return x + 200 }
fn main() i32 {
    (fn(u32) u32)[2] fns
    fns[0] = f1
    fns[1] = f2
    return (i32)(fns[1](20) - fns[0](10))
}
