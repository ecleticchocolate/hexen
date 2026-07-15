//@ expect val 8
alias X = u8
fn main() i32 {
    i32 r = 0
    { alias X = u64  r = (i32)sizeof(X) }
    return r
}
