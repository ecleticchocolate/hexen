//@ expect val 2
fn first_gt(i32* a,i32 n,i32 v)i32{for i32 i=0 to n{if a[i]>v{return i}} return -1} fn main()i32{i32[5] a={3,1,4,1,5} return first_gt(&a[0],5,3)}
