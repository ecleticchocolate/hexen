//@ expect val 5
// The case that proves the check is not merely "the last if needs an else": an
// exhaustive enum match covers every value, but the lowering still puts a CONDITION on
// its final arm (`else if (tag == 1) { ... }`), so the node has no false_block. A naive
// structural walk rejects this valid program; the exhaustive_tail flag is what tells it
// apart from a bare `if`.
enum E { i32 A  None }
fn get(E e) i32 {
    match e {
        .A(x) { return x }
        .None { return 0 }
    }
}
fn main() i32 { E e = .A(5)  return get(e) }
