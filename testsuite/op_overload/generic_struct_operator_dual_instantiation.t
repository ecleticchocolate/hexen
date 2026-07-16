//@ expect stdout
//@ | c.v=7 z.v=4.000000
extern fn printf(u8* fmt, ...) i32
struct Box[T] { T v }
impl Box[T] {
    fn __add(Box[T] other) Box[T] {
        return { .v = self.v + other.v }
    }
}
fn main() i32 {
    Box[i32] a = { .v = 3 }
    Box[i32] b = { .v = 4 }
    Box[i32] c = a + b

    Box[f64] x = { .v = 1.5 }
    Box[f64] y = { .v = 2.5 }
    Box[f64] z = x + y

    printf("c.v=%d z.v=%f\n", c.v, z.v)
    return 0
}
