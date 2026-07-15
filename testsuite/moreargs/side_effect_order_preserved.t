//@ expect val 136
i32[10] log
i32 logn = 0
fn mark(i32 tag) i32 { log[logn] = tag  logn = logn + 1  return tag }
fn sum8(i32 a,i32 b,i32 c,i32 d,i32 e,i32 f,i32 g,i32 h) i32 { return a+b+c+d+e+f+g+h }
fn main() i32 {
    i32 r = sum8(mark(1),mark(2),mark(3),mark(4),mark(5),mark(6),mark(7),mark(8))
    i32 order_ok = 1
    for i32 k = 0 to 8 {
        if log[k] != k + 1 { order_ok = 0 }
    }
    return order_ok * 100 + r
}
