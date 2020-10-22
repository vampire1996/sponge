#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

//template <typename... Targs>
//void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) , bytes({}),state({}) , unassembled_bytes_cnt(0) , input_end_index(-1),last_read_index(0){
     for(size_t i=0;i<capacity;i++)
     {
        bytes.push_back(0);
	state.push_back(false);
     }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    /*
     n1=_output.bytes_read()
     n2=_output.bytes_written()
StreamReassembler:
     relative idx 0------------------------------------------capacity
     absolute idx n1-----------------------------------------capacity+n1    
_output:
                  n1--------------n2
data:		  
               relative idx index-n1-------index-n1+data.length 
               absolute idx index-------------index+data.length   
     */	
    size_t i,j;	
    size_t len=index+data.length();
    size_t n1=_output.bytes_read();
    size_t n2=_output.bytes_written();
    len=len>n1+_capacity?n1+_capacity:len;
    /*
     when a byte in _outout is read,it does not belong to ResseambledStream anymore
     so it needs to be poped from bytes and state,also we need to keep the capacity of them of _capacity
     */
    for(size_t k=last_read_index;k<n1;k++)
    {
        bytes.pop_front();
	bytes.push_back(0);
	state.pop_front();
	state.push_back(false);
    }
    last_read_index=n1;
    if(index<=n2)
    {
      std::string s="";
      for(i=n2,j=n2-n1;i<len;i++,j++)
      {
         bytes[j]=data[i-index]; 
	 s+=bytes[j];
         if(state[j])unassembled_bytes_cnt--;
         state[j]=false;//after wrote to _output bytes[j] is unused	 
      }      
      for(;i<_capacity+n1 && state[j];j++,i++)
      { 
         s+=bytes[j];
	 state[j]=false;
	 unassembled_bytes_cnt--;
      }
      _output.write(s);
     // if(data=="c"&&index==2) std::cout<<s<<" "<<i<<" "<<j<<" "<<_output.bytes_written()<<" "<<n1<<" "<<n2<<" "<<state[3]<<" "<<bytes[j]<<std::endl;
      if(eof) input_end_index=index+data.length();
      if(i==input_end_index)_output.end_input();
    }
    else
    {	    
       for(i=index,j=index-n1;i<len;i++,j++)
       {
         bytes[j]=data[i-index];
	 if(!state[j])
	 {
	   state[j]=true;
	   unassembled_bytes_cnt++;
	 }
       
       }
       if(eof) input_end_index=index+data.length();
      // if(data=="d"&&index==3) std::cout<<i<<j<<std::endl;
    }
}

size_t StreamReassembler::unassembled_bytes() const { return unassembled_bytes_cnt; }

bool StreamReassembler::empty() const { return unassembled_bytes()==0; }
