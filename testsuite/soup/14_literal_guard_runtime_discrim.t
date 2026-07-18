//@ expect stdout
//@ | 100
//@ | 7
//@ | -1
extern fn printf(u8* fmt, ...) i32;
enum Opt[T] { T Some None }
fn check(Opt[i32] o) i32 {
    match o {
        .Some(0) { return 100 }
        .Some(v) { return v }
        .None { return -1 }
    }
}
fn main() i32 {
    printf("%d\n", check(.Some(0)))
    printf("%d\n", check(.Some(7)))
    printf("%d\n", check(.None))
    return 0
}
