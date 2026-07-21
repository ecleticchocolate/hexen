//@ expect val 100010001
// The tag ASSERTS the nominal kind: struct/enum/union each match only their own.
struct S { i32 a }
enum   E { i32 A }
union  U { i32 i  u8* s }
fn ps[T]() i32 { match T { struct M { return 1 } else { return 0 } } return -1 }
fn pe[T]() i32 { match T { enum   M { return 1 } else { return 0 } } return -1 }
fn pu[T]() i32 { match T { union  M { return 1 } else { return 0 } } return -1 }
fn main() i32 {
    i32 a = ps[S]()*100 + ps[E]()*10 + ps[U]()
    i32 b = pe[S]()*100 + pe[E]()*10 + pe[U]()
    i32 c = pu[S]()*100 + pu[E]()*10 + pu[U]()
    return a*1000000 + b*1000 + c
}
