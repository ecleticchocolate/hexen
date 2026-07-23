//@ expect val 300
struct Node {
    i32 val
    Node* next
}

fn make_default_list() Node* {
    Node* n2 = new Node{.val = 200, .next = null}
    Node* n1 = new Node{.val = 100, .next = n2}
    return n1
}

fn sum_list(Node* head) i32 {
    i32 sum = 0
    Node* curr = head
    while (curr) {
        sum = sum + curr.val
        curr = curr.next
    }
    return sum
}

fn process_list(Node* head = make_default_list()) i32 {
    return sum_list(head)
}

// Global const evaluated at COMPILE TIME via constexpr.c interpreter!
const i32 COMPTIME_SUM = process_list()

fn main() i32 {
    return COMPTIME_SUM
}
