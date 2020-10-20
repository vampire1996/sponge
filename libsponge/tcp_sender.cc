#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , RTO{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const {// cout<<_bytes_in_flight<<endl; 
       	return _bytes_in_flight; }


/*
 find the min value of three values below
 _stream.buffer_size()
 TCPConfig::MAX_PAYLOAD_SIZE
 cur_window_size

 the size of header do nothing to the payload,but syn and fin will change the value of sequence number--segment.length_in_sequence_space()
  */
void TCPSender::fill_window() {
    // cout<<RTO<<" "<<timer_ms<<endl;

    // if receiver says window_size is zero,set cur_window_size to 0 because
    // we can not just stuck there,and send nothing	
    // window
    // ackno-- ackno+window_size
    uint64_t right=checkpoint+(cur_window_size==0?1:cur_window_size);
    if(right>_next_seqno && !timer_run)
    {
       //if there is room in the window,some bytes must be sent	    
       //every time a segment containing data is sent,restart timer		
       	timer_run=true;
        timer_ms=0;
     
    }
    // uint64_t right=checkpoint+cur_window_size;
     for(uint16_t size=right-_next_seqno;_next_seqno<right;size=right-_next_seqno)
   // for(uint16_t size=cur_window_size==0?1:cur_window_size;size>0;)
    {
         TCPSegment segment;
	 TCPHeader header;
	 Buffer payload;
	 size_t payload_size=TCPConfig::MAX_PAYLOAD_SIZE;
	 if(!syn_loaded) //syn has no payload
	 {
            syn_loaded=true;
            header.syn=true;	    
	   // size--;
	 }
	 else{    // cout<<_stream.buffer_size()<<" "<<size<<endl; 		 
            payload_size=payload_size<_stream.buffer_size()?payload_size:_stream.buffer_size();
	    payload_size=payload_size<size?payload_size:size;
	    Buffer p(_stream.read(payload_size));
	    payload=p;
	    size-=payload_size;
         }
         if(!fin_loaded && size>0 && _stream.input_ended() && _stream.buffer_size()==0)
	 {
	    // fin can own payload	 
	    fin_loaded=true;
	    header.fin=true;
	    _input_end_index=_next_seqno+segment.length_in_sequence_space();

	 }
         // if no payload no fin no syn just break
         if(payload_size==0 && !header.fin && ! header.syn) break;
       	 header.seqno=next_seqno();
         segment.header()=header;
	 segment.payload()=payload;
         _segments_out.push(segment);	
	 _segments_not_ack.push(segment);
	 // size-=payload_size;
	 _bytes_in_flight+=segment.length_in_sequence_space();
         _next_seqno+=segment.length_in_sequence_space();
    }
    // cout<<_next_seqno<<" "<<_stream.bytes_written()<<endl; 
    
    
        

}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 

	bool flag=false; //if new data acked,flag=true
        // if receiver says window_size is zero,set cur_window_size to 0 because
	// we can not just stuck there,and send nothing
	cur_window_size=window_size;
        RTO=_initial_retransmission_timeout;
        consecutive_retrans_cnt=0;
        while(!_segments_not_ack.empty() &&unwrap(_segments_not_ack.front().header().seqno+_segments_not_ack.front().length_in_sequence_space() , _isn,checkpoint)<=unwrap(ackno,_isn,checkpoint))
	{
	   checkpoint+=_segments_not_ack.front().length_in_sequence_space();	
	   _bytes_in_flight-=_segments_not_ack.front().length_in_sequence_space();	
          // cout<<_bytes_in_flight<<endl;
    	   //if ack>sequence number of last byte in one segment,pop it		
	   _segments_not_ack.pop();
	   flag=true;
	}

	//new data acked and there are outstanding data to be acked,restart timer
        if(flag && !_segments_not_ack.empty())
	{
	   timer_run=true;
	   timer_ms=0;
	}	
         
	//if new space has opened up,fill the window again
	fill_window();

	// if all segments has been acked,close timer
	if(_segments_not_ack.empty() && _stream.input_ended())
	{
           _fully_acked=true;  		
	   timer_run=false;
	}

	// if acking something has not been sent return false 
        if(unwrap(ackno,_isn,checkpoint)>_next_seqno)return false;
        return true;	
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 	
   // cout<<ms_since_last_tick<<endl;
    if(timer_run)
    {
       timer_ms+=ms_since_last_tick;
    }
    if(timer_ms>=RTO) timer_run=false;
    // cout<<timer_ms<<" "<<timer_run<<" "<<RTO<<endl;
    // if _segments_not _ack is empty,it means that everything has been acked
    if(!timer_run && !_segments_not_ack.empty() ) 
    {
	//cout<<timer_ms<<" "<<_segments_not_ack.empty()<<endl;    
       //send the earliest segment that has not been fully acked	    
       _segments_out.push(_segments_not_ack.front());
       if(cur_window_size!=0)
       {
          consecutive_retrans_cnt++;
          RTO*=2;
       }
       timer_run=true;
       timer_ms=0;
    }
    
}

unsigned int TCPSender::consecutive_retransmissions() const { return consecutive_retrans_cnt; }

void TCPSender::send_empty_segment() {
      TCPSegment segment;
      TCPHeader header;
      header.seqno=next_seqno();
      segment.header()=header;
      _segments_out.push(segment);
}
