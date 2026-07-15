//@ expect val 43
enum Status { u32 Ok  bool Err }
struct Result[T] { T data  Status st }
fn main() i32 {
    Result[u32] r = {.data = 42, .st = .Ok{1}}
    match r.st {
        .Ok{v} { return (i32) r.data + (i32) v }
        .Err{e} { return -1 }
    }
    return -2
}
