//@ expect err unpack cannot bind an enum value
enum Shape { f32 Circle  f32 Square  Empty }
fn main() i32 {
    Shape s = .Circle{2.0}
    unpack .Circle{r} = s
    return 0
}
