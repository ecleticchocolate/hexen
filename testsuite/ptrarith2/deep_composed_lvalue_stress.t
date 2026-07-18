//@ expect val 610
struct Pair { i32 a  i32 b }
fn main() i32 {
    i32[5] arr = {10, 20, 30, 40, 50}
    Pair[3] pairs = {{.a=1,.b=2}, {.a=3,.b=4}, {.a=5,.b=6}}
    i32* base = &arr[0]
    i32 i = 1
    i32 j = 2
    // Deref of pointer arithmetic where the OFFSET is itself a non-trivial
    // expression, writing through it.
    *(base + (i*j - 1)) = 100
    // Read back through the same composed lvalue, write into a struct
    // array element's field.
    pairs[i].b = *(base + (i*j - 1)) + arr[0]
    // Chained assignment through two different places at once.
    i32 x = 0
    i32 y = 0
    x = y = pairs[i].b - 10
    // Address-of a deep place (struct-array-field), write through it.
    i32* pj = &pairs[j].a
    *pj = x + y
    return arr[1] + pairs[1].b + pairs[2].a + x + y
}
