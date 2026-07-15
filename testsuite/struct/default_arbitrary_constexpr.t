//@ expect val 110
fn compute(i32 n) i32 {
    if n <= 1 { return n }
    return compute(n - 1) + compute(n - 2)
}
const i32 MULT = 2
struct Config {
    i32 id = compute(10) * MULT // 55 * 2 = 110
    f32 scale = 1.5
}
fn main() i32 {
    Config c = {.scale = 2.0}
    return c.id
}
