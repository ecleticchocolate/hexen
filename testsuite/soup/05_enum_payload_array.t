//@ expect stdout
//@ | triple: 10 20 30
extern fn printf(u8* fmt, ...) i32;

enum Reading {
    i32[3] Triple
    i32 Single
}

fn main() i32 {
    Reading r = .Triple({10, 20, 30})
    match r {
        .Triple({a, b, c}) { printf("triple: %d %d %d\n", a, b, c) }
        .Single(v) { printf("single: %d\n", v) }
    }
    return 0
}
