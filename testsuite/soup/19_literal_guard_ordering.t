//@ expect stdout
//@ | 11
//@ | 22
//@ | 99
//@ | 0
extern fn printf(u8* fmt, ...) i32;
enum Opt[T] { T Some None }
fn name(Opt[i32] a) i32 {
    match a {
        .Some{1} { return 11 }
        .Some{2} { return 22 }
        .Some{v} { return v }
        .None { return 0 }
    }
}
fn main() i32 {
    printf("%d\n", name(.Some{1}))
    printf("%d\n", name(.Some{2}))
    printf("%d\n", name(.Some{99}))
    printf("%d\n", name(.None))
    return 0
}
