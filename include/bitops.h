#ifdef CONFIG64
#define BITS_PER_LONG 64        // nếu build 64-bit → long = 64 bit
#else
#define BITS_PER_LONG 32        // nếu không → long = 32 bit
#endif /* CONFIG64 */

#define BITS_PER_BYTE 8         // 1 byte = 8 bit

#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))  
// chia n/d nhưng làm tròn lên

#define BIT(nr) (1U << (nr))   
// tạo số có bit thứ nr = 1 (32-bit)

#define BIT_ULL(nr) (1ULL << (nr)) 
// tạo số có bit thứ nr = 1 (64-bit)

#define BIT_MASK(nr) (1UL << ((nr) % BITS_PER_LONG)) 
// mask bit nr trong 1 word

#define BIT_WORD(nr) ((nr) / BITS_PER_LONG) 
// xác định bit nr nằm ở word thứ mấy

#define BIT_ULL_MASK(nr) (1ULL << ((nr) % BITS_PER_LONG_LONG)) 
// mask bit cho 64-bit word

#define BIT_ULL_WORD(nr) ((nr) / BITS_PER_LONG_LONG) 
// xác định word index cho 64-bit

#define BITS_TO_LONGS(nr) DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long)) 
// cần bao nhiêu biến long để chứa nr bit

#define BIT_ULL_MASK(nr) (1ULL << ((nr) % BITS_PER_LONG_LONG)) 
// (lặp lại macro trên - có thể dư)

#define BIT_ULL_WORD(nr) ((nr) / BITS_PER_LONG_LONG) 
// (lặp lại macro trên - có thể dư)

/*
 * Create a contiguous bitmask starting at bit position @l and ending at
 * position @h. For example
 * GENMASK_ULL(39, 21) gives us the 64bit vector 0x000000ffffe00000.
 */
// tạo mask liên tục từ bit l đến h

#define GENMASK(h, l) \
(((~0U) << (l)) & (~0U >> (BITS_PER_LONG  - (h) - 1))) 
// tạo bitmask từ bit l → h

#define NBITS2(n) ((n&2)?1:0) 
// kiểm tra bit thứ 1 có bật không

#define NBITS4(n) ((n&(0xC))?(2+NBITS2(n>>2)):(NBITS2(n))) 
// đếm số bit trong 4 bit

#define NBITS8(n) ((n&0xF0)?(4+NBITS4(n>>4)):(NBITS4(n))) 
// đếm số bit trong 8 bit

#define NBITS16(n) ((n&0xFF00)?(8+NBITS8(n>>8)):(NBITS8(n))) 
// đếm số bit trong 16 bit

#define NBITS32(n) ((n&0xFFFF0000)?(16+NBITS16(n>>16)):(NBITS16(n))) 
// đếm số bit trong 32 bit

#define NBITS(n) (n==0?0:NBITS32(n)) 
// đếm tổng số bit = 1 trong số n

#define EXTRACT_NBITS(nr, h, l) ((nr&GENMASK(h,l)) >> l) 
// lấy các bit từ l → h trong nr rồi shift về phải