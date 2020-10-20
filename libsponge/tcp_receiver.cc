#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

//template <typename... Targs>
//void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    bool eof=false;
    std::string data=""; 
    std::string_view sv=seg.payload().str();    
    uint64_t index=0;
    uint64_t checkpoint=_reassembler.stream_out().bytes_written();
    WrappingInt32 seq_number=seg.header().seqno;
    bool syn_flag=seg.header().syn;
    bool fin_flag=seg.header().fin;    
    bool flag=false;
   // std::cout<<_capacity<<endl;
    // SYN  bytes_stream FIN
    // if ISN hasn't been set yet,a segment is acceptable if(and only if) it has the SYN bit set
    if(!syn_received && !syn_flag) return false;
    if(syn_flag)
    {
       flag=true;	    
       syn_received=true;	    
       isn=seq_number; 
       //isn+1 is because that isn is not wrote to byte stream
       index=unwrap(isn+1,isn,0);
       //if(sv.length()>0)sv=sv.substr(1);
    }	    
    else
    {
       uint64_t n=_reassembler.stream_out().bytes_written();
       index=unwrap(seq_number,isn,checkpoint);
       //if payload overlaps some part of the window,flag=true or the payload is empty-- an ack 
       if(!(index-1>=n+window_size() || index-1+sv.length()<=n) ) flag=true;
    }
    if(fin_flag)
    {
       flag=true;	    
       fin_received=true;	    
       eof=true;
       // if(sv.length()>0)sv=sv.substr(0,sv.length()-1);
    }
    // data=sv.data();//when sv.data() meet null,the return string ends so it is not right 
    for(uint64_t i=0;i<sv.length();i++) data+=sv[i];
   // cout<<data<<" "<<sv.length()<<endl;
    _reassembler.push_substring(data,index-1,eof);
    return flag;
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(!syn_received) return std::nullopt; 
    // ISN=SYN=24 received 25 26 27 bytes_written()=3  3+1=4 ackno=24+4=28
    uint64_t n=_reassembler.stream_out().bytes_written();
    //cout<<isn<<" "<<n<<" "<<wrap(n+1,isn)<<endl;
    //when all bytes are reassembled(arrives eof) and fin is received
    //ack +1 because fin is included
    if(_reassembler.stream_out().input_ended() && fin_received)n++;
    return wrap(n+1,isn);
}

size_t TCPReceiver::window_size() const {
        size_t size=_capacity-_reassembler.stream_out().buffer_size();    
        return size;  
        //	return size==0?1:size; 
}

