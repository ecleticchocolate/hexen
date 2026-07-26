//@ expect err function argument
// Two anon structs that both NAME their fields differently are different types,
// and a CALL must not perform a conversion assignment already rejects.
//
// This was a silent hole: `b = a` errored, but `f(a)` converted screen x/y into
// lat/lon with no diagnostic -- the two fields have the same types, so only the
// names carry the meaning that was being discarded.
fn takes_coords(struct{f32 lat  f32 lon} p) f32 { return p.lat }
fn main() i32 {
    struct{f32 x  f32 y} screen
    screen.x = 42.0
    return (i32)takes_coords(screen)
}
