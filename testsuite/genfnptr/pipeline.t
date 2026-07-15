//@ expect val 36
fn identity[T](T x) T { return x }
fn double[T](T x) T { return x + x }
fn square[T](T x) T { return x * x }
struct Pipeline { fn(u32) u32 s1  fn(u32) u32 s2  fn(u32) u32 s3 }
fn run(Pipeline* p, u32 x) u32 { return p.s3(p.s2(p.s1(x))) }
fn main() i32 {
    Pipeline p = {.s1 = double, .s2 = square, .s3 = identity}
    return (i32) run(&p, 3)
}
