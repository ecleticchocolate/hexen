//@ expect val 5
// `super` is implemented with zero validation on which of struct/enum/union
// it appears in -- it splices the same way everywhere, whether or not
// embedding "makes sense" for that shape. A union just overlaps the promoted
// fields with everything else the same way it always overlaps fields; an
// enum treats the promoted fields as ordinary variants, tag and all. This
// test only pins that both PARSE and RUN without error; embedding into an
// enum in particular has no useful semantics here, deliberately.
struct A {
   u32 x
}
union U {
   super A base
   u32 y
}
fn main() i32 {
    U u
    u.x = 5
    return u.x
}
