//@ expect val 84
struct Inner { i32 v }
struct Outer { Inner i }
impl Inner { fn doubled() i32 { return self.v * 2 } }
impl Outer { fn inner() Inner { return self.i } }
fn make() Outer { return {.i = {.v = 21}} }
fn main() i32 {
    i32 a = make().i.v
    i32 b = make().inner().doubled()
    i32 c = make().inner().v
    return a + b + c
}
