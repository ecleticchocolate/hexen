//@ expect val 60
struct Pair { i32 sum i32 prod }
fn calc(i32 a, i32 b, i32 c, i32 d, i32 e, i32 f, i32 g) Pair {
    return {.sum = f, .prod = 0}
}
fn main() i32 {
    Pair r = calc(1,2,3,4,5,60,7)
    return r.sum
}
