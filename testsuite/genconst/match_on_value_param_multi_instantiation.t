//@ expect val 179
// Multiple arms over a value-param scrutinee, dispatched correctly across four
// distinct instantiations (each N folds independently, no cross-talk).
fn classify[u32 N]() i32 {
    match N {
        0 { return 10 }
        1 { return 20 }
        5 { return 50 }
        else { return 99 }
    }
}
fn main() i32 {
    return classify[0]() + classify[1]() + classify[5]() + classify[3]()
    // 10 + 20 + 50 + 99
}
