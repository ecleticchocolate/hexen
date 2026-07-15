//@ expect val 66
struct Mat[T, u32 R, u32 C] {
    T[R][C] d
}

impl Mat[T, R, C] {
    fn add(Mat[T, R, C]* other) Mat[T, R, C] {
        Mat[T, R, C] res
        for u32 i = 0 to R {
            for u32 j = 0 to C {
                res.d[i][j] = self.d[i][j] + other.d[i][j]
            }
        }
        return res
    }
}

fn main() i32 {
    Mat[i32, 2, 3] m1
    m1.d[0][0] = 1; m1.d[0][1] = 2; m1.d[0][2] = 3
    m1.d[1][0] = 4; m1.d[1][1] = 5; m1.d[1][2] = 6

    Mat[i32, 2, 3] m2
    m2.d[0][0] = 10; m2.d[0][1] = 20; m2.d[0][2] = 30
    m2.d[1][0] = 40; m2.d[1][1] = 50; m2.d[1][2] = 60

    Mat[i32, 2, 3] m3 = m1.add(&m2)
    return m3.d[1][2]
}
