//@ expect val 8
fn main() i32 {
    match u64* {
        P* { return (i32)sizeof(P) }
        else { return 111 }
    }
}
