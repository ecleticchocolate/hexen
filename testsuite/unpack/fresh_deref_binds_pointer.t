//@ expect val 5
// A fresh `*px` leaf binds `px` as a pointer INTO the projected slot (auto& /
// by-ref), consistent with how `.x = px` binds a fresh `px`. Reading/writing
// `*px` goes through the pointer. (For a by-VALUE scrutinee the pointer aims at
// the match temp; write-through to the ORIGINAL needs a pointer scrutinee, as in
// the for-unpack over yielded Node* case -- see forin/fresh_deref_writes_through.)
struct Point { i32 x  i32 y }
fn main() i32 {
    Point p = {.x=1, .y=2}
    unpack {.x=*px, .y=*py} = p
    *px = 5
    return *px     // read back through px
}
