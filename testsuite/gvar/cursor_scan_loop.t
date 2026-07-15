//@ expect val 2
u8* g u32 gp=0 fn dig(u8 c)bool{return c>=48 && c<=57} fn main()i32{ g="42" gp=0 while dig(g[gp]){gp=gp+1} return (i32)gp }
