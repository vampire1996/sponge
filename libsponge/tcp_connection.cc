#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().buffer_size();}
	
	
	
size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
   if(seg.header().rst)
   {
      rst_happen(false);	   
      return ;
   }

   // a segment is accetable only if syn is received
   if(!syn_received && !seg.header().syn) return;

  // if(_outbound_fully_acked && !seg.header().fin) return;

   // update ackno and win_size of sender   
   if(seg.header().ack && !_sender.ack_received(seg.header().ackno,seg.header().win)) // sender cares about segment's ackno
   {
   	      
      // tepSender says that an ackno was invalid
      //make sure a segment is sent with updated window size and ackno
      //dont ack ack
      if(_sender.segments_out().empty() && seg.payload().size()!=0 )
      {
         _sender.send_empty_segment();
      }

   }
     

   if(!_receiver.segment_received(seg)) // receiver cares about segment's seqno
   {
      //received segment did not overlap the window and was unacceptable
      //make sure a segment is sent with updated window size and ackno
      //dont ack ack
      if(_sender.segments_out().empty() && seg.payload().size()!=0 )
      {
         _sender.send_empty_segment();
      }

   }
   else 
   {
      // if _receiver.segments_received(seg) return true 
      // it means that seg occupies a sequence number
      // so it must be acked,it segments_out is empty,we need to fill in a empty segment
      if(seg.header().syn)
      {
	 syn_received=true;	 
	 // the port is listening rather than connect to other port
         // need to reply a syn ack
	 if(!syn_sent) connect();
      }
      // if fin is not fully acked or segment's header fin =true
      // must sent a reply segment
      if(_sender.segments_out().empty() &&  ( !_outbound_fully_acked || seg.header().fin) )

      {
	 cout<<_outbound_fully_acked<<" "<<_sender.stream_in().input_ended()<<" "<<_sender.stream_in().buffer_size()<<" "<<seg.header().fin<<endl;     
         _sender.send_empty_segment();
      }
   }
   // cout<<seg.header().syn<<" "<<syn_sent<<endl;
   

   // updata the first outbound segment's ackno and win_size
   fill_ack_win();
  // cout<<_receiver.ackno().has_value()<<endl;
   //if(_receiver.ackno().has_value()) cout<<_receiver.ackno().value()<<" "<<_segments_out.front().header().ackno<<endl;
   _time_since_last_segment_received=0;
  // if(!_sender.segments_out().empty() ) cout<<_sender.segments_out().front().header().syn<<endl;
   // fin received from remote peer before TCPConnection has reached EOF on its outbound stream  ,no need to linger after stream finish,so set it to false
   if(seg.header().fin && !_sender.stream_in().input_ended())
   {
	   // cout<<seg.header().fin<<endl;
	   _linger_after_streams_finish=false;
   }

   // check if outbound stream full acked
   if(_sender.stream_in().input_ended() && _sender.stream_in().buffer_size()==0 )
   {
     // _outbound_fully_acked=true;
   }
 

}

// three cases conncetion is down
// (1) rst=true
// (2) inbounded fully assembled and has ended (fin recv) linger_after_stream_finish=false  and sender stream is fully acked
// if linger_after_stream_finish is false,input has ended
// (3) sender stream is fully acked and linger time out (no segment received in 10*_cfg.rt_timeout)

bool TCPConnection::active() const {
     if(rst) return false;
     if(!_linger_after_streams_finish&& _outbound_fully_acked) return false;
     if(_linger_timeout) return false;
     return true;     
 }

size_t TCPConnection::write(const string &data) {
    size_t len=_sender.stream_in().write(data);
    _sender.fill_window();
    fill_ack_win();
   // cout<<_segments_out.size()<<endl;
    return len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    _time_since_last_segment_received+=ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if(_sender.consecutive_retransmissions()>TCPConfig::MAX_RETX_ATTEMPTS)
    {
        rst_happen(true);
    }
    cout<<_outbound_fully_acked<<" "<<_cfg.rt_timeout<<" "<<_time_since_last_segment_received<<endl;
    if(_outbound_fully_acked && _time_since_last_segment_received>=10*_cfg.rt_timeout) _linger_timeout=true;
    fill_ack_win(); // certain segments may need to be resent if timeout
}

void TCPConnection::end_input_stream() {
   _sender.stream_in().end_input();
   _sender.fill_window();
   fill_ack_win();

}

void TCPConnection::connect() {

  // send a segment with ack
  _sender.fill_window();
  fill_ack_win();
  syn_sent=true;
 
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
	    rst_happen(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

// fill the ackno and window_size to sender's outbound segemnt
void TCPConnection::fill_ack_win()
{
   //cout<<_receiver.ackno().value()<<endl;	
  //if there are segemnts to be sent and receiver's ackno is not null
  if(!_sender.segments_out().empty() && _receiver.ackno().has_value())
   {
     int size=_sender.segments_out().size();
     for(int i=0;i<size;i++)
     {     
     _sender.segments_out().front().header().ack=true;	     
     _sender.segments_out().front().header().ackno=_receiver.ackno().value();
      
     //header().win is uint_16 but _receiver.window_size() is size_t
     //so the biggest window size is numeric_limits<char16_t>
     _sender.segments_out().front().header().win=_receiver.window_size()>numeric_limits<char16_t>::max()?numeric_limits<char16_t>::max() :_receiver.window_size();
     _sender.segments_out().push(_sender.segments_out().front());
     _sender.segments_out().pop();
     }
   }


}

// the entire connection should be aborted,deal with rst 
void TCPConnection::rst_happen(bool is_sending)
{  
   rst=true;
   _sender.stream_in().set_error();
   _receiver.stream_out().set_error();
   if(is_sending) // if sending a rst
   {
      //clear all outbound segemnt	   
      while(!_sender.segments_out().empty()) _sender.segments_out().pop();
      //generate an empty segemnt
      _sender.send_empty_segment();
      // set rst
      _sender.segments_out().front().header().rst=true;
   }
}

