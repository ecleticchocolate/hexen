//@ expect stdout
//@ | DEL 99
// The single-object form still destroys exactly one object. `new T` allocates
// no cookie, so `delete` must not go looking for one.
extern fn printf(u8* fmt, ...) i32
struct R { i32 id }
impl R { fn __delete() void { printf("DEL %d\n", self.id) } }
fn main() i32 {
    R* one = new R{.id = 99}
    delete one
    return 0
}
