//@ expect err operator not defined on
struct Box[T, u32 N] { T items }
enum Wrapper { Box[(fn(i32) i32)[2], 1] Fns  None }
fn add_one(i32 x) i32 { return x + 1 }
fn add_two(i32 x) i32 { return x + 2 }
fn main() i32 {
    Wrapper a = .Fns{ { .items = {add_one, add_two} } }
    Wrapper b = .Fns{ { .items = {add_one, add_two} } }
    return (i32)(a == b)
}
