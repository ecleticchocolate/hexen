//@ expect stdout
//@ | sum=50
extern fn printf(u8* fmt, ...) i32;
extern fn malloc(u64 n) void*;
struct Node { i32 val; Link next; }
enum Link { Node* More  None }

fn sum(Link l) i32 {
    match l {
        .More{n} { return (*n).val + sum((*n).next) }
        .None { return 0 }
    }
}

fn main() i32 {
    Node* n2 = (Node*)malloc(sizeof(Node))
    (*n2).val = 30
    (*n2).next = .None

    Node* n1 = (Node*)malloc(sizeof(Node))
    (*n1).val = 20
    (*n1).next = .More{n2}

    Link head = .More{n1}
    printf("sum=%d\n", sum(head))
    return 0
}
