
#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

//template <typename... Targs>
//void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return isn+n;
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    //high 32bits of checkpoint is checkpoint&mask
    //there are three choice num1 num2 num3 
    //the difference between these three numbers and checkpoint are
    //d1 d2 d3,we need to find the closet one	
    WrappingInt32 absolute(n-isn);    
    uint64_t absolute_value=absolute.raw_value();
    uint64_t mask=0xfffffffful<<32;
    uint64_t high32=checkpoint&mask;
    uint64_t num1=(high32+(1ul<<32))|absolute_value;
    uint64_t num2= high32 | absolute_value;
    uint64_t num3=(high32-(1ul<<32))|absolute_value;
    uint64_t d1=num1-checkpoint;
    uint64_t d2=num2>checkpoint?num2-checkpoint:checkpoint-num2;
    uint64_t d3=checkpoint-num3;	  
    uint64_t res=0;  
    //num3 is not needed
    if(high32==0) return d1<d2?num1:num2;
    if(d1<d2)
    {
       if(d1<d3) res=num1;
       else res=num3;
    }
    else
    {
       if(d2<d3) res=num2;
       else res=num3;
    }
    return res;
}
