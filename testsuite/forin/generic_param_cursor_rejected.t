//@ expect err for-in iterable must be
enum Option[T] { T Some  None }
struct Node { i32 v  Node* next }
struct List { Node* head }
struct Cur { Node* p }
impl List { fn begin() Cur { return {.p = self.head} } }
impl Cur { fn next() Option[i32*] { if self.p == null { return .None } Node* n = self.p  self.p = n.next  return .Some{&n.v} } }
fn sum_it[T](T container) i32 { i32 s = 0  for i32 x in container { s = s + x }  return s }
fn main() i32 { Node a = {.v=1,.next=null}  List l = {.head=&a}  return sum_it[List](l) }
