//@ expect stdout
//@ | destructor v=42
// `delete p` calls p's __delete() (if its pointee type defines one) before
// the actual free -- the language's real destructor/free-exit-point hook.
// Reuses ordinary method-call dispatch for the __delete() call; the actual
// pointer release stays the existing, unmodified AST_DELETE codegen (a plain
// free(), always -- __delete is cleanup logic, never a replacement for
// releasing the memory itself).
extern fn printf(u8* fmt, ...) i32
struct Resource { i32 v }
impl Resource {
    fn __delete() void {
        printf("destructor v=%d\n", self.v)
    }
}
fn main() i32 {
    Resource* r = new Resource{.v = 42}
    delete r
    return 0
}
