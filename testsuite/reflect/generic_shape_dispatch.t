//@ expect val 854
fn unwrap_size[T](T x) u32 {
    match T {
        P* { return sizeof(P) }
        E[N] { return N }
        else { return sizeof(T) }
    }
}
fn main() i32 {
    u64 v = 0
    u64* p = &v
    i16[5] arr
    i32 a = 0
    return (i32)unwrap_size(p) * 100 + (i32)unwrap_size(arr) * 10 + (i32)unwrap_size(a)
}
