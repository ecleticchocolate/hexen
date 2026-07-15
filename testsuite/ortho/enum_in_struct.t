//@ expect val 77
enum Status { u32 Ok  bool Err }
struct Resp { Status st  u32 code }
fn main() i32 {
    Resp r = {.st = .Ok{77}, .code = 200}
    match r.st {
        .Ok{v} { return (i32) v }
        .Err{e} { return -1 }
    }
    return -2
}
