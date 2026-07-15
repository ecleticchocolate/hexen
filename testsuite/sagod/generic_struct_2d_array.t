//@ expect val 26
struct Cell[T]{T val u32 tag} fn main()i32{Cell[u32][2][2] grid u32 acc=0 for u32 i=0 to 2{for u32 j=0 to 2{grid[i][j].val=i*10+j grid[i][j].tag=1 acc=acc+grid[i][j].val+grid[i][j].tag}} return (i32)acc}
