//@ expect val 66
struct Row{u32[4] cells u32 sum} struct Grid{Row[3] rows} fn main()i32{Grid g u32 total=0 for u32 r=0 to 3{u32 s=0 for u32 c=0 to 4{g.rows[r].cells[c]=r*4+c s=s+g.rows[r].cells[c]} g.rows[r].sum=s total=total+g.rows[r].sum} return (i32)total}
