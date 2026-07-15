//@ expect err match scrutinee must be an enum value, a non-float primitive, or a struct/array
fn main() i32 {
    f64 x = 5.0
    match x {
        1.0 { return 1 }
        else { return 0 }
    }
    return 0
}
