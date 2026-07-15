//@ expect val 12
fn main()i32{ i32 x=0 while x<10{ i32 y=0 while true{y=y+1 if y==3{break}} x=x+y } return x }
