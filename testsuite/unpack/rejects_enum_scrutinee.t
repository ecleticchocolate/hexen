//@ expect err enum-variant patterns are allowed here
enum Shape { f32 Circle  f32 Square  Empty }
fn main() i32 {
    Shape s = .Circle(2.0)
    unpack .Circle(r) = s
    return 0
}
