//@ expect stdout
//@ | IntBox:
//@ |   has get() returning 4 bytes
//@ | FloatBox:
//@ |   has get() returning 8 bytes
//@ | Adder:
//@ |   has add(4 bytes) -> 4 bytes
// Wildcards bind THROUGH a method signature, because the signature is an ordinary
// TYPE_FUNCTION run through the ordinary reflect_unify. No wildcard logic was
// written for impl patterns -- it falls out of the type grammar.
extern fn printf(u8* fmt, ...) i32
struct IntBox { i32 v }
impl IntBox { fn get() i32 { return self.v } }
struct FloatBox { f64 v }
impl FloatBox { fn get() f64 { return self.v } }
struct Adder { i32 v }
impl Adder { fn add(i32 n) i32 { return self.v + n } }
fn describe[T](T v) {
    match T {
        impl { fn get() R } { printf("  has get() returning %d bytes\n", sizeof(R)) }
        impl { fn add(A) B } { printf("  has add(%d bytes) -> %d bytes\n", sizeof(A), sizeof(B)) }
        else { printf("  nothing\n") }
    }
}
fn main() i32 {
    IntBox i = {.v=1}   FloatBox f = {.v=2.0}   Adder a = {.v=3}
    printf("IntBox:\n")    describe(i)
    printf("FloatBox:\n")  describe(f)
    printf("Adder:\n")     describe(a)
    return 0
}
