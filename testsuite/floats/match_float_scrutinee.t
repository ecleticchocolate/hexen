//@ expect val 42
// A float scrutinee in `match` is legal (a previous implementation arbitrarily
// rejected floats; the value engine lifts that restriction). Arm literals compare
// by value; duplicate-arm detection still applies. f32 and f64 both work.
fn pick(f64 x) i32 {
    match x {
        1.0 { return 1 }
        2.5 { return 42 }
        else { return 0 }
    }
}
fn pickf(f32 y) i32 {
    match y {
        1.5 { return 100 }
        else { return 7 }
    }
}
fn main() i32 {
    if pick(2.5) != 42 { return 90 }
    if pick(9.0) != 0  { return 91 }
    if pickf(1.5) != 100 { return 92 }
    return pick(2.5)
}
