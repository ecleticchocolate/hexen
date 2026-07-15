//@ expect stdout
//@ | global const : 10 20 30
//@ | local  const : 10 20 30
//@ | local  f64[] : 1.500000 2.500000 3.500000
//@ | local  struct: 1.250000 2.750000
// REGRESSION GUARD. A `const` AGGREGATE declared at *function* scope used to read
// back all zeros: the aggregate const path gives the symbol SYM_GLOBAL storage
// (it needs an address, unlike a scalar const, which splices a literal per use
// site), but the image initializer walked ONLY the global scope table -- so a
// const written inside a function body had its folded bytes computed correctly
// and then never copied into the image. Silent wrong values, no diagnostic.
// Fixed by keying emission on "carries folded bytes" (Global_RegisterForEmit)
// rather than on scope-table membership. See docs/KNOWN_BUGS.md.
extern fn printf(u8* fmt, ...) i32
struct FS { f64 a  f64 b }
const u32[3] G = {10, 20, 30}
fn main() i32 {
    printf("global const : %d %d %d\n", G[0], G[1], G[2])
    const u32[3] L = {10, 20, 30}
    printf("local  const : %d %d %d\n", L[0], L[1], L[2])
    const f64[3] F = {1.5, 2.5, 3.5}
    printf("local  f64[] : %f %f %f\n", F[0], F[1], F[2])
    const FS S = {.a = 1.25, .b = 2.75}
    printf("local  struct: %f %f\n", S.a, S.b)
    return 0
}
