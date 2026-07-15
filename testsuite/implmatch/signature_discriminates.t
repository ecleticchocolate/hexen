//@ expect stdout
//@ | Good:
//@ |   -> good
//@ | Bad:
//@ |   -> <opaque>
//@ | None:
//@ |   -> <opaque>
// The signature is MATCHED, not just the name: `Bad` has a to_string() but it
// returns u32 instead of u8*, so it must NOT match. A name-only predicate
// (has_method("to_string")) would match Bad and then produce garbage.
extern fn printf(u8* fmt, ...) i32
struct Good { u32 x }
impl Good { fn to_string() u8* { return "good" } }
struct Bad  { u32 x }
impl Bad  { fn to_string() u32 { return 42 } }
struct None { u32 x }
fn show[T](T v) {
    match T {
        impl { fn to_string() u8* } { printf("  -> %s\n", v.to_string()) }
        else { printf("  -> <opaque>\n") }
    }
}
fn main() i32 {
    Good g = {.x=1}   Bad b = {.x=2}   None n = {.x=3}
    printf("Good:\n")  show(g)
    printf("Bad:\n")   show(b)
    printf("None:\n")  show(n)
    return 0
}
