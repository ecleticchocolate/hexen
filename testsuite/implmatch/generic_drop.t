//@ expect stdout
//@ | drop(Buf):
//@ |     Buf freed
//@ | drop(Handle):
//@ |     Handle closed (fd=7)
//@ | drop(Plain):
//@ |   (nothing, correctly)
// A universal drop() over any type that HAS a free(). Buf and Handle are unrelated
// and declared no relationship to anything -- they participate by having the method.
extern fn printf(u8* fmt, ...) i32
extern fn malloc(u64 n) u8*
extern fn free(u8* p) void
fn drop[T](T v) {
    match T {
        impl { fn free() } { v.free() }
        else { }
    }
}
struct Buf { u8* p }
impl Buf { fn free() void { free(self.p)  printf("    Buf freed\n") } }
struct Handle { u32 fd }
impl Handle { fn free() void { printf("    Handle closed (fd=%d)\n", self.fd) } }
struct Plain { u32 x }
fn main() i32 {
    Buf b = {.p = malloc(16)}
    Handle h = {.fd = 7}
    Plain p = {.x = 1}
    printf("drop(Buf):\n")     drop(b)
    printf("drop(Handle):\n")  drop(h)
    printf("drop(Plain):\n")   drop(p)
    printf("  (nothing, correctly)\n")
    return 0
}
