//@ expect val 20
fn add1(i32 x) i32 { return x + 1 }
fn double(i32 x) i32 { return x * 2 }
fn triple(i32 x) i32 { return x * 3 }
fn main() i32 {
    (fn(i32) i32)[3] arr = {add1, double, triple}
    (fn(i32) i32)* p = &arr[0]
    p = p + 1
    fn(i32) i32 g = *p
    return g(10)
}
