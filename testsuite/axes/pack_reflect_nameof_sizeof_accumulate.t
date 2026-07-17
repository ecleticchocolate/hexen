//@ expect val 1
extern fn printf(u8* fmt, ...) i32
// Reflection (match on type, nameof, offsetof) crossed with variadic-pack
// peeling: walk a struct's fields printing each field's name via nameof
// while ALSO summing sizeof(H) via the value carried in the recursive call --
// i.e. reflection metadata (name/offset) and pack-peeled VALUES used together
// in the same recursive walk, not just one or the other as existing tests do.
fn total_size[Orig, Walk, u32 N](u32 acc) u32 {
    match Walk {
        struct { H head  Rest... rest } {
            printf("field %u: %s (size %u)\n", N, nameof(Orig, N), (u32)sizeof(H))
            return total_size[Orig, Rest, N + 1](acc + (u32)sizeof(H))
        }
        struct {} { return acc }
    }
}
struct Mixed { i32 a  f64 b  u8 c }
fn main() i32 {
    u32 total = total_size[Mixed, Mixed, 0](0)
    // i32(4) + f64(8) + u8(1) = 13
    if total == 13 { return 1 }
    return 0
}
