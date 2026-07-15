//@ expect err without a return
// The same hole through a bare `if`: the false path falls off the end. No `match`
// involved -- this is where the garbage actually comes from.
fn pick(bool c) u32 {
    if c { return 100 }
}
fn main() u32 { return pick(false) }
