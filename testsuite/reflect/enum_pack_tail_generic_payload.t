//@ expect stdout
//@ | count=4
//@ | first Vec at index=0
// Enum pack-tail peeling combined with generic payload types: a concrete
// generic instantiation (`List[u32]`), a wildcard-typed generic (`Box[E]`),
// and a const-generic with both a type and value wildcard (`Vec[E, N]`) all
// unify correctly as an enum variant's payload type, at any position in the
// variant list, through the same Rest... recursion the struct case already had.
extern fn printf(u8* fmt, ...) i32
struct Vec[T, u32 N] { T[N] e }
enum Deep { Vec[i32, 4] Fixed4  Vec[i32, 8] Fixed8  bool Flag  u8* Name }

fn count[T](u32 acc) u32 {
    match T {
        enum { H; Rest... } { return count[Rest](acc + 1) }
        enum {  } { return acc }
    }
}

fn find_const_generic[T](u32 acc) u32 {
    match T {
        enum { Vec[E, N]; Rest... } { return acc }
        enum { H; Rest... } { return find_const_generic[Rest](acc + 1) }
        enum {  } { return 999 }
    }
}

fn main() i32 {
    printf("count=%d\n", count[Deep](0))
    printf("first Vec at index=%d\n", find_const_generic[Deep](0))
    return 0
}
