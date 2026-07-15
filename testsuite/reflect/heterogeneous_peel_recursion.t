//@ expect val 105
fn peel[T](T x) u32 {
    match T { P* { return 100 + peel(*x) } E[N] { return N } else { return 0 } }
}
fn main() i32 { i32[5] a  i32[5]* pa = &a  return (i32)peel(pa) }
