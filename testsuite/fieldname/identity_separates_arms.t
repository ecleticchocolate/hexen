//@ expect val 12
struct P { i32 alpha  u8 beta }
struct Q { i32 gamma  u8 beta }
fn which[T]() i32 {
    match T {
        struct { A alpha  B beta } { return 1 }
        struct { A gamma  B beta } { return 2 }
        else { return -1 }
    }
}
fn main() i32 { return which[P]() * 10 + which[Q]() }
