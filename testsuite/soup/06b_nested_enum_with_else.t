//@ expect stdout
//@ | Wrapped A: 5
//@ | Wrapped B: 9
//@ | none
extern fn printf(u8* fmt, ...) i32;
enum Inner { i32 A  i32 B }
enum Outer { Inner Wrapped  None }

fn main() i32 {
    Outer o1 = .Wrapped{.A{5}}
    Outer o2 = .Wrapped{.B{9}}
    Outer o3 = .None

    match o1 {
        .Wrapped{.A{v}} { printf("Wrapped A: %d\n", v) }
        .Wrapped{.B{v}} { printf("Wrapped B: %d\n", v) }
        else { printf("none\n") }
    }
    match o2 {
        .Wrapped{.A{v}} { printf("Wrapped A: %d\n", v) }
        .Wrapped{.B{v}} { printf("Wrapped B: %d\n", v) }
        else { printf("none\n") }
    }
    match o3 {
        .Wrapped{.A{v}} { printf("Wrapped A: %d\n", v) }
        .Wrapped{.B{v}} { printf("Wrapped B: %d\n", v) }
        else { printf("none\n") }
    }
    return 0
}
