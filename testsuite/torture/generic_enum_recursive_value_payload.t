//@ expect val 142
enum List[T, u32 N] {
    T[N] Leaf
    List[T, N]* Node
}

fn main() i32 {
    List[i32, 2]* root = new[1] List[i32, 2]
    *root = .Node( new[1] List[i32, 2] )
    
    List[i32, 2]* child = root.Node
    *child = .Leaf( {42, 100} )
    
    i32 sum = 0
    match *root {
        .Leaf(l) { }
        .Node(n) {
            match *n {
                .Leaf(l2) { sum = l2[0] + l2[1] }
                .Node(n2) { }
            }
        }
    }
    
    delete root.Node
    delete root
    return sum
}
