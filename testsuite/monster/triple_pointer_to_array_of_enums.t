//@ expect val 42
enum State { u32 Active  bool Inactive }
fn main() i32 {
    State[2] arr = { .Active{42}, .Inactive{true} }
    State[2]* p1 = &arr
    State[2]** p2 = &p1
    State[2]*** p3 = &p2
    match (***p3)[0] {
        .Active{v} { return (i32) v }
        .Inactive{b} { return -1 }
    }
    return -2
}
