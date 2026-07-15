//@ expect val 41
fn mk(u32 x) u32 { return x + 1 }
fn main() i32 {
    (fn(u32) u32)[2] arr
    arr[0] = mk
    arr[1] = mk
    (fn(u32) u32)[2]* p = &arr
    return (i32)((*p)[1](40))
}
