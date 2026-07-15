//@ expect val 28
fn sum7(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f, i32 g) i32 { return a+b+c+d+e+f+g }
fn main() i32 {
    fn(i32,i32,i32,i32,i32,i32,i32) i32 fp = sum7
    return fp(1,2,3,4,5,6,7)
}
