//@ expect val 60
fn clamp(i32 x,i32 lo,i32 hi)i32{if x<lo{return lo} if x>hi{return hi} return x} fn main()i32{return clamp(-5,0,10)*100+clamp(5,0,10)*10+clamp(15,0,10)}
