//@ expect val 9
fn add1(i32 x) i32 { return x + 1 }
fn mul2(i32 x) i32 { return x * 2 }
fn sub3(i32 x) i32 { return x - 3 }
fn main() i32 {
    (fn(i32)i32)[3] ops = {add1, mul2, sub3}
    i32 val = 5
    val = ops[0](val)
    val = ops[1](val)
    val = ops[2](val)
    return val
}
