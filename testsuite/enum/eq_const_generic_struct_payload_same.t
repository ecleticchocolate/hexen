//@ expect val 1
struct Point { i32 x  i32 y }
struct Box[T, u32 N] { T[N] items }
enum Wrapper { Box[Point, 2] Points  None }
fn main() i32 {
    Wrapper a = .Points{ { .items = { {.x=1,.y=2}, {.x=3,.y=4} } } }
    Wrapper b = .Points{ { .items = { {.x=1,.y=2}, {.x=3,.y=4} } } }
    return (i32)(a == b)
}
