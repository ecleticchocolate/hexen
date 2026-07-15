//@ expect err recursive layout
struct Node{Node child} fn main()i32{Node n={} return 0}
