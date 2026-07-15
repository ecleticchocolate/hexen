//@ expect val 899
// TITAN A — runtime breadth: every type width, operator, control-flow, pointer
// form, heap, string, cast, struct/enum/array/fnptr/generic, composed.

struct Vec[T] { T x  T y }
enum Shape { Vec[i32] Point  u32 Circle  None }
struct Registry { (fn(u32) u32)[3] ops  u32 count }

fn add1(u32 x) u32 { return x + 1 }
fn mul2(u32 x) u32 { return x * 2 }
fn sqr(u32 x)  u32 { return x * x }

fn dist2(Vec[i32] v) i32 { return v.x * v.x + v.y * v.y }

fn classify(i32 n) Shape {
    if n < 0 { return .None }
    if n == 0 { return .Circle{0} }
    return .Point{ {.x = n, .y = n} }
}

fn main() i32 {
    i64 total = 0

    // --- integer widths + casts + overflow wrap ---
    u8  a8  = 250
    a8 = a8 + 10            // wraps: 260 % 256 = 4
    u16 b16 = 0xFFFF
    i16 c16 = -100
    total = total + (i64)a8 + (i64)((u32)b16 - 65000) + (i64)c16   // 4 + 535 + (-100) = 439

    // --- all arithmetic + bitwise + shifts + modulo ---
    u32 m = (100 + 20 - 5) * 2 / 3      // 76
    m = m % 50                          // 26
    u32 bits = (0xF0 | 0x0F) & 0xFF     // 255
    bits = (bits >> 4) ^ 0x0A           // 15 ^ 10 = 5
    bits = bits << 2                    // 20
    total = total + (i64)m + (i64)bits  // +26 +20 = 485

    // --- bool / logical / comparison / unary ---
    bool t = (m > 20) && !(bits < 10) || false
    if t { total = total + 1 }          // +1 = 486

    // --- while + break + continue ---
    u32 i = 0
    u32 wsum = 0
    while true {
        i = i + 1
        if i > 10 { break }
        if i % 2 == 0 { continue }
        wsum = wsum + i                 // 1+3+5+7+9 = 25
    }
    total = total + (i64)wsum           // +25 = 511

    // --- for + nested + array (multi-dim) ---
    u32[3][3] grid
    for u32 r = 0 to 3 {
        for u32 c = 0 to 3 { grid[r][c] = r * 3 + c }
    }
    total = total + (i64)(grid[2][2] - grid[0][0])   // 8 - 0 = +8 = 519

    // --- pointers, double pointers, ptr-to-array, deref write ---
    u32 px = 100
    u32* pp = &px
    *pp = *pp + 5                       // px = 105
    u32** ppp = &pp
    **ppp = **ppp + 5                   // px = 110
    u32[4] arr = {1, 2, 3, 4}
    u32[4]* parr = &arr
    (*parr)[1] = 99
    total = total + (i64)px + (i64)arr[1]   // 110 + 99 = +209 = 728

    // --- heap new/delete ---
    u32* heap = new[5] u32
    for u32 k = 0 to 5 { heap[k] = k * k }   // 0,1,4,9,16
    u32 hsum = 0
    for u32 k = 0 to 5 { hsum = hsum + heap[k] }   // 30
    delete heap
    total = total + (i64)hsum           // +30 = 758

    // --- struct by value + generic struct + fn returning struct via call ---
    Vec[i32] v = {.x = 3, .y = 4}
    total = total + (i64)dist2(v)       // 9+16 = +25 = 783

    // --- enum + match + generic-enum-payload + nested struct in enum ---
    Shape s = classify(7)
    match s {
        .Point{pt} { total = total + (i64)(pt.x + pt.y) }   // 7+7 = +14
        .Circle{r} { total = total - 1 }
        .None { total = total - 2 }
    }                                   // = 797

    // --- fnptr array in struct, dispatched ---
    Registry reg = {.ops = {add1, mul2, sqr}, .count = 3}
    u32 dispatched = 0
    for u32 j = 0 to 3 { dispatched = dispatched + reg.ops[j](4) }  // 5 + 8 + 16 = 29
    total = total + (i64)dispatched     // +29 = 826

    // --- string literal + indexing (ASCII) ---
    u8* str = "ABC"
    total = total + (i64)str[0]         // A is 65, +65 = 891

    // --- contextual enum + sizeof ---
    total = total + (i64)sizeof(u64)    // +8 = 899

    return (i32) total                  // 899
}
