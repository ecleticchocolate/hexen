//@ expect val 200
fn classify(u32[3] a) i32 {
    match a {
        {1, 2, 3} { return 100 }
        {9, 9, 9} { return 200 }
        else { return -1 }
    }
}
fn main() i32 {
    u32[3] a2 = {9,9,9}
    return classify(a2)
}
