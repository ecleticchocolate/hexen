//@ expect val 30
struct Vector { i32 x  i32 y }
struct Box[T] { T val }
fn main() i32 {
    struct Box[struct Vector] b = {.val = {.x = 10, .y = 20}}
    return b.val.x + b.val.y
}
