//@ expect val 50
fn main() i32 {
    u32* arr = new[4] u32
    arr[0] = 10; arr[1] = 20; arr[2] = 30; arr[3] = 40
    i32 r = (i32)(arr[0] + arr[3])
    delete arr
    return r
}
