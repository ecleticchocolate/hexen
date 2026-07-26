//@ expect stdout
//@ | DEL 0
//@ | DEL 1
//@ | DEL 2
// `delete[] p` runs the destructor on EVERY element, not just element 0.
// Plain `delete` on a new[] buffer only ever destroyed the first one, which is
// why a container had to hand-roll its own destroy loop.
extern fn printf(u8* fmt, ...) i32
struct R { i32 id }
impl R { fn __delete() void { printf("DEL %d\n", self.id) } }
fn main() i32 {
    R* buf = new[3] R
    buf[0].id = 0
    buf[1].id = 1
    buf[2].id = 2
    delete[] buf
    return 0
}
