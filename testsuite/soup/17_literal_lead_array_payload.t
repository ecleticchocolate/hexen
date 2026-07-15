//@ expect stdout
//@ | 1050
//@ | 6
//@ | 42
extern fn printf(u8* fmt, ...) i32;
enum R { i32[3] T  i32 S }
fn desc(R r) i32 {
    match r {
        .T{{10, b, c}} { return 1000 + b + c }
        .T{{a, b, c}}  { return a + b + c }
        .S{v} { return v }
    }
}
fn main() i32 {
    printf("%d\n", desc(.T{{10, 20, 30}}))
    printf("%d\n", desc(.T{{1, 2, 3}}))
    printf("%d\n", desc(.S{42}))
    return 0
}
