//@ expect val 7
enum Val { Cell Cons  i64 Int }
struct Cell { i64 a  i64 b }
fn main() i32 {
    Cell c = {.a = 3, .b = 4}
    Val v = .Cons{c}
    match v {
        .Cons{c} { return (i32)(c.a + c.b) }
        .Int{n} { return -1 }
    }
    return -2
}
