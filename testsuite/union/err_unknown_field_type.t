//@ expect err Expected field type in union
union U { NoSuchType x }
fn main() i32 {
    return 0
}
