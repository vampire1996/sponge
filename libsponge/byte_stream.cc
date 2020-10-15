#include "byte_stream.hh"
#include <iostream>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

//template <typename... Targs>
//void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity):stream_capacity(capacity),buf(""),bytes_read_cnt(0),bytes_written_cnt(0),input_flag(false){ 
  // this-> capacity=capacity;
  // buf="";
  // bytes_read_cnt=0;
  // bytes_written_cnt=0;
  // input_flag=false;
}

size_t ByteStream::write(const std::string &data) {
   unsigned len=0;
   if(data.length()+buf.length()<stream_capacity)
   {
      len=data.length();
      buf+=data;
   }
   else
   {
      len=stream_capacity-buf.length();
      buf+=data.substr(0,len);
   }
   bytes_written_cnt+=len;
   return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
std::string ByteStream::peek_output(const size_t len) const {
    unsigned l=len<=buf.length()?len:buf.length();
    return buf.substr(0,l);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
        unsigned l=len<=buf.length()?len:buf.length();
        bytes_read_cnt+=l;	
	buf=buf.substr(l,buf.length()-l); }
//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {	
    std::string res=peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() {input_flag=true;}

bool ByteStream::input_ended() const { return input_flag; }

size_t ByteStream::buffer_size() const { return buf.length(); }

bool ByteStream::buffer_empty() const { return buf.length()==0; }

bool ByteStream::eof() const {
       // if(input_flag) cout<<bytes_written_cnt<<" "<<bytes_read_cnt<<endl;
       return input_flag&&buffer_empty();    
       //	return (input_flag&&(bytes_written_cnt==bytes_read_cnt));

}


size_t ByteStream::bytes_written() const { return bytes_written_cnt; }

size_t ByteStream::bytes_read() const { return bytes_read_cnt; }

size_t ByteStream::remaining_capacity() const {
	return stream_capacity-buf.length(); }
