//@ expect stdout
//@ | declaration=42
//@ | assignment=42
//@ | argument=42
//@ | return=42
//@ | struct field=42
// A source type that declares __cast is CONVERTED rather than rejected, at
// every assignability site -- one rule in check_assignable, not a special case
// per site. Primitives already coerced implicitly; this extends the same
// "try a conversion, then bail" rule to any type that opted in.
//
// The layouts deliberately differ (the wanted value is Wrap's SECOND field), so
// a result of 42 proves the conversion RAN. Merely relaxing the check without
// inserting a cast yielded 111 -- the raw bytes reinterpreted.
extern fn printf(u8* fmt, ...) i32
struct Small { u32 v }
struct Wrap { u32 junk  u32 real }
impl Wrap { fn __cast[T]() T { Small s  s.v = self.real  return s } }
struct Holder { Small s }
fn takes(Small s) u32 { return s.v }
fn gives(Wrap w) Small { return w }
fn main() i32 {
    Wrap w
    w.junk = 111
    w.real = 42
    Small d = w
    printf("declaration=%d\n", (i32)d.v)
    Small a
    a = w
    printf("assignment=%d\n", (i32)a.v)
    printf("argument=%d\n", (i32)takes(w))
    printf("return=%d\n", (i32)gives(w).v)
    Holder h = {.s = w}
    printf("struct field=%d\n", (i32)h.s.v)
    return 0
}
