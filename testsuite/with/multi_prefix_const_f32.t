//@ expect val 6
with const f32 {
    PI    = 3.0
    E     = 2.0
    SQRT2 = 1.0
}
fn main() i32 { return (i32)(PI + E + SQRT2) }
