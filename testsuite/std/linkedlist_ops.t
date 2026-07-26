//@ use std/option.t std/iterator.t std/linkedlist.t
//@ expect stdout
//@ | len=3 empty=0
//@ | idx1=2
//@ | front=1 back=3
//@ | get1=2 oob=none
//@ | set=1 val=99
//@ | popback=3 popfront=1 len=1
//@ | insert=1 len=3
//@ | remove=5 len=2
//@ | iter=111
//@ | eq=1 neq=0
//@ | reversed=12
//@ | cleared=0
extern fn printf(u8* fmt, ...) i32
fn main() i32 {
    LinkedList[i32] l = LinkedList[i32].create()
    l.push_back(1)  l.push_back(2)  l.push_back(3)
    printf("len=%d empty=%d\n", (i32)l.len(), (i32)l.is_empty())
    printf("idx1=%d\n", l[1])
    match l.front() { {.Some = f} { match l.back() { {.Some = b} { printf("front=%d back=%d\n", f, b) } .None {} } } .None {} }
    match l.get(1) { {.Some = v} { printf("get1=%d ", v) } .None {} }
    match l.get(99) { {.Some = v} { printf("BAD\n") } .None { printf("oob=none\n") } }
    printf("set=%d val=%d\n", (i32)l.set(1, 99), l[1])
    match l.pop_back()  { {.Some = v} { printf("popback=%d ", v) } .None {} }
    match l.pop_front() { {.Some = v} { printf("popfront=%d len=%d\n", v, (i32)l.len()) } .None {} }
    printf("insert=%d ", (i32)l.insert_at(0, 5))
    l.push_back(12)
    printf("len=%d\n", (i32)l.len())
    match l.remove_at(0) { {.Some = v} { printf("remove=%d len=%d\n", v, (i32)l.len()) } .None {} }
    i32 s = 0
    for i32* p in l { s = s + *p }
    printf("iter=%d\n", s)
    LinkedList[i32] c = l.copy()
    printf("eq=%d neq=%d\n", (i32)(l == c), (i32)(l != c))
    l.reverse()
    printf("reversed=%d\n", l[0])
    l.clear()
    printf("cleared=%d\n", (i32)l.len())
    return 0
}
