//@ expect val 8
// Pack-tail peel where a peeled field (H) is itself a GENERIC struct
// instantiation, not a scalar -- sizeof/nameof must resolve through the
// concrete instantiation, not the generic base.
struct Box[T] { T v }
fn count_generic_fields[Walk, u32 N](u32 acc) u32 {
    match Walk {
        struct { H head  Rest... rest } {
            return count_generic_fields[Rest, N + 1](acc + (u32)sizeof(H))
        }
        struct {} { return acc }
    }
}
struct Combo { Box[i32] a  Box[i32] b }
fn main() i32 {
    return (i32)count_generic_fields[Combo, 0](0)
}
