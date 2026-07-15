//@ expect val 7
// `void` parses to PRIM_V, not PRIM_VOID (parser.c). Testing only PRIM_VOID silently
// demanded a return from every void function -- which is how the check first came out
// wrong, and broke every impl block with a `free()`.
struct C { u32 v }
impl C { fn bump() void { self.v = self.v + 1 } }
fn side() void { }
fn main() u32 {
    C c = {.v = 6}
    side()
    c.bump()
    return c.v
}
