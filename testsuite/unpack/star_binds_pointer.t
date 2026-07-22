//@ expect val 5
// `*name` pattern leaf binds `name` as a pointer INTO the slot (write-through,
// no copy). The one hardcoded pattern feature. Works in unpack and match alike.
struct Point { i32 x  i32 y }
fn main() i32 {
    Point p = {.x=1, .y=2}
    unpack {.x=*px, .y=*py} = p
    *px = 5
    return *px
}
