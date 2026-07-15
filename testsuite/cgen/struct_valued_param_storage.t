//@ expect val 104
struct Point { u32 x  u32 y }
struct Tagged[T, Point Origin] { T val }
fn main() i32 {
    Tagged[i32, {.x = 7, .y = 9}] t = {.val = 100}
    return (i32)(t.val + (i32)sizeof(t))   // 100 + 4 = 104
}
