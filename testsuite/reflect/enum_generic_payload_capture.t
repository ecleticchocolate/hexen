//@ expect val 81
enum Result[T, E] { T Ok  E Err }
fn main() i32 {
    match Result[u64, u8] {
        Result[OK, ER] { return (i32)sizeof(OK)*10 + (i32)sizeof(ER) }
        else { return 0 }
    }
}
