//@ expect val 888990
u8* g_s u32 g_pos=0 fn next()u8{u8 c=g_s[g_pos] g_pos=g_pos+1 return c} fn main()i32{ g_s="XYZ" return (i32)(next())*10000+(i32)(next())*100+(i32)(next()) }
