//@ expect val 0
struct Point { i32 x  i32 y }
enum Shape { Point Dot  u32 Circle }
fn main() i32 {
    Shape a = .Dot( {.x = 3, .y = 4} )
    Shape b = .Dot( {.x = 9, .y = 9} )
    return (i32)(a == b)
}
