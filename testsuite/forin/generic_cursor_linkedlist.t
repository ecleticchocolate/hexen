//@ expect val 60
// Regression: for-in's cursor protocol (begin() -> Cursor, Cursor.next() -> Option)
// must also resolve through the generic base name for an instantiated generic
// struct (List[i32]'s `begin` lives under `List_begin`, not `List_i32_begin`).
enum Option[T] { T Some  None }
struct Node { i32 v  Node* next }
struct List[T] { Node* head }
struct Cur { Node* p }
impl List[T] { fn begin() Cur { return {.p = self.head} } }
impl Cur { fn next() Option[i32*] { if self.p == null { return .None } Node* n = self.p  self.p = n.next  return .Some(&n.v) } }
fn main() i32 {
    Node c = {.v = 30, .next = null}
    Node b = {.v = 20, .next = &c}
    Node a = {.v = 10, .next = &b}
    List[i32] l = {.head = &a}
    i32 s = 0
    for i32 x in l { s = s + x }
    return s
}
