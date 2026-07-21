//@ expect val 2
// The three malformed pack declarations are rejected at parse time:
//   struct A[Ts..., u32 N]  -- pack must be LAST (nothing after it could ever
//                              receive an argument, since the pack absorbs all)
//   struct B[As..., Bs...]  -- at most one pack per list (two would make the
//                              split point ambiguous)
//   struct C[u32 Ns...]     -- a pack must be a TYPE param; a value param is
//                              pinned to one concrete type, so there is nothing
//                              for a bundle to be
// Each is checked by its own single-line source in the errors/ suite; this test
// just pins the working shape those errors guard.
struct Ok[u32 N, Ts...] { u32 tag  Ts rest }
fn main() i32 {
    Ok[1, i32, u8] a
    a.tag = 2
    return (i32)a.tag
}
