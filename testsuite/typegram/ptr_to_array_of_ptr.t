//@ expect val 6
fn main() i32 {
    u32 v = 6
    u32*[2] arr
    arr[0] = &v
    u32*[2]* pp = &arr
    return (i32)(*((*pp)[0]))
}
