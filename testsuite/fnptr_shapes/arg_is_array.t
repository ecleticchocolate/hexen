//@ expect val 6
fn sum3(i32[3] arr) i32 { return arr[0] + arr[1] + arr[2] }
fn main() i32 {
    fn(i32[3]) i32 f = sum3
    i32[3] vals = {1, 2, 3}
    return f(vals)
}
