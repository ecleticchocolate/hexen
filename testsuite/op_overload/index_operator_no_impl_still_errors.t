//@ expect err indexing a non-array, non-pointer
struct Vec { i32 x  i32 y }
fn main() i32 {
    Vec v = { .x = 5, .y = 9 }
    return v[0]
}
