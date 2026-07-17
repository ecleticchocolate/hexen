//@ expect val 25
enum Option[T] { T Some  None }
struct Node { i32 v  Node* next }
struct List { Node* head }
struct Cur { Node* p }
impl List { fn begin() Cur { return {.p = self.head} } }
impl Cur { fn next() Option[i32*] { if self.p == null { return .None } Node* n = self.p  self.p = n.next  return .Some{&n.v} } }
fn main() i32 {
    Node b = {.v=2,.next=null}  Node a = {.v=1,.next=&b}
    List l = {.head=&a}
    for i32 x in l { for i32* p in l { *p = *p + 1 } }   // inner runs twice (outer len), each pass +1 to both nodes -> +2 each: {3,4}
    i32 s = 0
    for i32 x in l { s = s + x * x }   // 3*3 + 4*4 = 9+16 = 25? recompute
    return s
}
