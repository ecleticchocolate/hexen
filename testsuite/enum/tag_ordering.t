//@ expect val 2
enum Dir { u32 North  u32 South  u32 East  u32 West }
fn tag(Dir d) u32 { return *(u32*)&d }
fn main() i32 {
    // tags: North=0, South=1, East=2, West=3
    Dir d = .East(0)
    return (i32) tag(d)
}
