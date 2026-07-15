//@ expect val 1205
enum Color { Red  Green  Blue }
struct Node[T, u32 CAP] { T[CAP] slots  Color tag  u32 len }
fn main() i32 {
    match fn(u8) Node[i16, 5] {
        fn(X) R {
            match R {
                Node[ELEM, CAP] { return (i32)sizeof(X)*1000 + (i32)sizeof(ELEM)*100 + (i32)CAP }
                else { return 0 }
            }
        }
        else { return 0 }
    }
}
