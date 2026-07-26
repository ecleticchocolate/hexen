//@ expect stdout
//@ | DEL
//@ | DEL
//@ | DEL
// A GENERIC element type must resolve to its monomorphized destructor.
// Method_Resolve alone returns the base TEMPLATE symbol, which emits a fixup to
// an uncompiled generic and destroys nothing -- delete[] silently no-opped on
// exactly the element types that need it most (a container of containers).
extern fn printf(u8* fmt, ...) i32
struct Buf[T] { T* data }
impl Buf[T] { fn __delete() void { printf("DEL\n") } }
fn main() i32 {
    Buf[u8]* arr = new[3] Buf[u8]
    delete[] arr
    return 0
}
