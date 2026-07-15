//@ expect val 3
with const u32 {
    RED   = 0
    GREEN = 1
    BLUE  = 2
}
fn main() i32 { return (i32)(RED + GREEN + BLUE) }
