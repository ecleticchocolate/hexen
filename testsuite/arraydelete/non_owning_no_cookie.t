//@ expect val 7
// An element type with NO destructor gets no cookie: the allocation and the
// free are byte-identical to before this feature existed, so a `new[N] u8`
// buffer is still a plain malloc pointer safe to hand to C.
struct Plain { i32 v }
fn main() i32 {
    Plain* p = new[4] Plain
    p[0].v = 7
    i32 r = p[0].v
    delete[] p
    u8* b = new[16] u8
    b[0] = 1
    delete[] b
    return r
}
