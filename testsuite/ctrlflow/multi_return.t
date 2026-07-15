//@ expect val 201
fn sign(i32 x)i32{if x>0{return 1} if x<0{return -1} return 0} fn main()i32{return sign(5)*100+sign(-3)*10+sign(0)+111}
