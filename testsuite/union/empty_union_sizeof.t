//@ expect val 0
union Empty { }
fn main() i32 {
    return (i32) sizeof(Empty)
}
