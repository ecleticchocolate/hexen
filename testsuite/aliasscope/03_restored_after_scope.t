//@ expect val 1
alias X = u8
fn main() i32 {
    { alias X = u64 }
    return (i32)sizeof(X)
}
