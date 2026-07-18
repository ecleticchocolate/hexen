//@ expect err error
enum T { u32 A  u32 B }
fn main() i32 {
    T t = .B(5)
    i32 r = match t { .A(x) { (i32) x }  .B(x) { (i32) x * 10 } }
    return r
}
