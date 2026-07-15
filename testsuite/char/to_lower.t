//@ expect val 10921
fn lower(u8 c)u8{if c>=65&&c<=90{return c+32} return c} fn main()i32{return (i32)lower(65)*100+(i32)lower(90)*10+(i32)lower(97)/97}
