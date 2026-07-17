//@ expect val 9
enum Option[T] { T Some  None }
struct Node { i32 v  Node* next }
struct List { Node* head }
struct Cur { Node* p }
impl List { fn begin() Cur { return {.p = self.head} } }
impl Cur { fn next() Option[i32*] { if self.p == null { return .None } Node* n = self.p  self.p = n.next  return .Some{&n.v} } }
fn main() i32 {
    Node b = {.v=2,.next=null}  Node a = {.v=1,.next=&b}
    List l = {.head=&a}
    i32[3] arr = {1,2,3}
    i32 s = 0
    for i32 x in l { for i32 y in arr { if y == 3 { continue } s = s + x * y } }
    return s
}
