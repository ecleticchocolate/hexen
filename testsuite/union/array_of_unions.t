//@ expect val 6
union U { i32 x  f32 y }
fn main() i32 {
    U[3] arr
    arr[0].x = 1
    arr[1].x = 2
    arr[2].x = 3
    return arr[0].x + arr[1].x + arr[2].x
}
