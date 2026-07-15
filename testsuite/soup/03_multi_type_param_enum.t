//@ expect stdout
//@ | Ok: 5
//@ | Err: 1
extern fn printf(u8* fmt, ...) i32;

enum Result[T, E] { T Ok  E Err }

fn divide(i32 a, i32 b) Result[i32, u8] {
    if b == 0 { return .Err{1} }
    return .Ok{a / b}
}

fn main() i32 {
    Result[i32, u8] r1 = divide(10, 2)
    Result[i32, u8] r2 = divide(10, 0)

    match r1 {
        .Ok{v} { printf("Ok: %d\n", v) }
        .Err{e} { printf("Err: %d\n", e) }
    }
    match r2 {
        .Ok{v} { printf("Ok: %d\n", v) }
        .Err{e} { printf("Err: %d\n", e) }
    }
    return 0
}
