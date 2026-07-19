//@ expect val 30
struct P { i32 x  i32 y }
fn copy[T](T v) T { auto c = v  return c }
fn main() i32 {
    auto a = copy(10)
    P p = {.x = 7, .y = 13}
    P p2 = copy(p)
    return a + p2.x + p2.y
}
