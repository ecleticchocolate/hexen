//@ expect val 21
struct Pt{f64 x f64 y} struct Poly{Pt[3] verts u32 n} fn main()i32{Poly p={.verts={{.x=1.0,.y=2.0},{.x=3.0,.y=4.0},{.x=5.0,.y=6.0}},.n=3} f64 sum=0.0 for u32 i=0 to 3{sum=sum+p.verts[i].x+p.verts[i].y} return (i32)sum}
