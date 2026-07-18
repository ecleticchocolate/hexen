//@ expect val 31
fn inc[T](T x) T { return x + 1 }
fn double[T](T x) T { return x + x }
enum Op { fn(i32) i32 F  None }
fn run_op(Op o, i32 x) i32 {
    match o { .F(f) { return f(x) }  .None { return 0 } }
    return 0
}
fn main() i32 {
    Op[3] ops = {.F(inc), .F(double), .None}
    i32 acc = 0
    for u32 i = 0 to 3 { acc = acc + run_op(ops[i], 10) }
    return acc
}
