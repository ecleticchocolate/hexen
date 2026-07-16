//@ expect val 0
// A struct with NO __delete must still `delete` cleanly (ordinary free, no
// destructor call attempted) -- the operator-method dispatch check must fall
// back cleanly, same as every other operator overload's own no-impl guard.
struct Plain { i32 v }
fn main() i32 {
    Plain* p = new Plain{.v = 1}
    delete p
    return 0
}
