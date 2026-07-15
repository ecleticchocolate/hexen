//@ expect val 25
struct Foo { i32 x }
impl Foo {
    fn x_plus(i32 n) i32 { return self.x + n }
}
fn main() i32 {
    Foo f = {.x = 10}
    i32 direct = f.x
    i32 via_method = f.x_plus(5)
    return direct + via_method
}
