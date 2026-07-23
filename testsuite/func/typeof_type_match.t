//@ expect val 1399
fn match_on_typeof[T](T val) i32 {
    match typeof(val) {
        i32 { return 100 }
        f32 { return 200 }
        else { return 300 }
    }
}

fn match_pattern_typeof[T, U](T val, U sample) i32 {
    match T {
        typeof(sample) { return 999 }
        else { return 111 }
    }
}

fn main() i32 {
    i32 v1 = match_on_typeof(42)           // matches i32 -> 100
    i32 v2 = match_on_typeof(3.14)         // matches f64 -> else 300
    i32 v3 = match_pattern_typeof(10, 20)  // typeof(20) is i32, T is i32 -> matches typeof(sample) -> 999

    return v1 + v2 + v3 // 100 + 300 + 999 = 1399
}
