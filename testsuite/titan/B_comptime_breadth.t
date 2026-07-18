//@ expect val 383
// TITAN B — COMPTIME breadth: the same span of features, all folded into one
// constant at compile time. Proves constexpr covers every closed-term axis the
// runtime does: widths, operators, control flow, recursion, structs, arrays,
// enums, match, generics, aggregate args, fn-returning-aggregate, floats.

struct Vec[T] { T x  T y }
enum Opt[T] { T Some  None }
struct Acc { u32 a  u32 b }

fn fib_step(Acc acc, u32 n) Acc {           // recursion + struct-by-value + aggregate return
    if n == 0 { return acc }
    return fib_step({.a = acc.b, .b = acc.a + acc.b}, n - 1)
}
fn fib(u32 n) u32 { return fib_step({.a = 0, .b = 1}, n).a }   // fib(10) = 55

fn dot(Vec[u32] v) u32 { return v.x * v.x + v.y * v.y }        // generic struct arg

fn maybe(u32 x) Opt[u32] {                  // fn returning generic enum (contextual)
    if x > 3 { return .Some(x * 10) }
    return .None
}

fn compute() u32 {
    u32 total = 0

    // arithmetic + bitwise + shift + modulo + casts + widths
    u32 m = ((100 + 20 - 5) * 2 / 3) % 50              // 26
    u32 bits = (((0xF0 | 0x0F) & 0xFF) >> 4 ^ 0x0A) << 2   // 20
    u8  w = 250
    w = w + 10                                         // wrap -> 4
    total = total + m + bits + (u32)w                 // 26+20+4 = 50

    // bool + logical + unary + if/else chain
    bool t = (m > 20) && !(bits < 10)
    if t { total = total + 1 } else { total = total - 1 }   // +1 = 51

    // while + break + continue
    u32 i = 0
    while true {
        i = i + 1
        if i > 5 { break }
        if i == 3 { continue }
        total = total + i                              // 1+2+4+5 = 12  -> 63
    }

    // for + array + index read/write
    u32[4] a = {10, 20, 30, 40}
    a[0] = a[3] - a[1]                                 // 40-20 = 20
    u32 asum = 0
    for u32 k = 0 to 4 { asum = asum + a[k] }          // 20+20+30+40 = 110
    total = total + asum                               // -> 173

    // recursion + struct accumulator (fib)
    total = total + fib(10)                            // +55 = 228

    // generic struct arg
    Vec[u32] v = {.x = 3, .y = 4}
    total = total + dot(v)                             // +25 = 253

    // fn returning generic enum + match + array of them
    Opt[u32][3] opts = { maybe(5), maybe(2), maybe(7) }   // Some{50}, None, Some{70}
    for u32 j = 0 to 3 {
        match opts[j] { .Some(s) { total = total + s }  .None { } }   // +50 +70 = +120 -> 373
    }

    // nested generic + field chain
    Vec[Vec[u32]] nested = {.x = {.x = 1, .y = 2}, .y = {.x = 4, .y = 8}}
    total = total + nested.x.y + nested.y.x            // 2 + 4 = +6 -> 379

    // sizeof
    total = total + (u32)sizeof(u32)                   // +4 -> 383

    return total
}

const u32 TITAN = compute()
fn main() i32 { return (i32) TITAN }   // 383
