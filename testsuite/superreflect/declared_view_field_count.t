//@ expect val 8
struct Base { u32 tag }
struct Derived { super Base info  u32 extra }
fn sum[T]() u64 {
    match T {
        struct { H; Rest... } { return sizeof(H) + sum[Rest]() }
        struct {  } { return 0 }
    }
}
fn main() i32 { return (i32) sum[Derived]() }
