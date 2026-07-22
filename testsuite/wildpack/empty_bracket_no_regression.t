//@ expect val 1
// A bare tagged pattern with NO bracket list at all (`struct M`, not even
// `[]`) must still work -- guards the app_pack_idx default-init fix: calloc's
// zero-init must never be misread as "pack at index 0" when no `...` was
// ever written.
struct Plain { i32 x }
fn f[S]() i32 { match S { struct M { return 1 } else { return 0 } } return -2 }
fn main() i32 { return f[Plain]() }
