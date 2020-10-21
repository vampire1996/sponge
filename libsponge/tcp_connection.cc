#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity();}
	
	
	
size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) { 
   //cerr<<"before segment_received "<<"rst: "<<seg.header().rst<<" syn: "<<seg.header().syn<<" fin: "<<seg.header().fin<<" "<<sender_state()<<" "<<receiver_state()<<"\n";	
   //if(seg.header().ack)cerr<<"ack: "<<seg.header().ackno<<" "<<seg.payload().size()<<"\n";
   if(!_active) return;
   _time_since_last_segment_received=0;

   if(seg.header().rst)
   {
      if(_sender.in_syn_sent() && !seg.header().ack) return ;	  
      rst_happen(false);	   
      return ;
   }

   // a segment is accetable only if syn is received
   if(_receiver.in_listen() && !seg.header().syn) return;


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
	 // the port is listening rather than connect to other port
         // need to reply a syn ack
	 if(_sender.in_closed()) connect();
      }
      // if fin is not fully acked or segment's header fin =true
      // must sent a reply segment
      if(_sender.segments_out().empty() &&  ( !_sender.in_fin_acked() || seg.header().fin) )
      {
         _sender.send_empty_segment();
      }
   }
  
  
   fill_and_update(false);
 
   // cerr<<"after segment_received "<<"rst: "<<seg.header().rst<<" syn: "<<seg.header().syn<<" fin: "<<seg.header().fin<<" "<<sender_state()<<" "<<receiver_state()<<"\n";	

}

// three cases conncetion is down
// (1) rst=true
// (2) inbounded fully assembled and has ended (fin recv) linger_after_stream_finish=false  and sender stream is fully acked
// if linger_after_stream_finish is false,input has ended
// (3) sender stream is fully acked and linger time out (no segment received in 10*_cfg.rt_timeout)

bool TCPConnection::active() const {
     /*	
     if(rst) return false;
     if(!_linger_after_streams_finish&& _outbound_fully_acked) return false;
     if(_linger_timeout) return false;
     return true;*/
     return _active;	
 }

size_t TCPConnection::write(const string &data) {
    size_t len=_sender.stream_in().write(data);
    //cerr<<"write "<<len<<"\n";
    fill_and_update(false);
    return len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    if(!_active) return;	
    //if(_time_since_last_segment_received==0) cerr<<_sender.segments_out().size()<<" \n";
    _time_since_last_segment_received+=ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if(_sender.consecutive_retransmissions()>TCPConfig::MAX_RETX_ATTEMPTS)
    {
        rst_happen(true);
    }
    fill_and_update(false); // certain segments may need to be resent if timeout
}

void TCPConnection::end_input_stream() {
   _sender.stream_in().end_input();
   fill_and_update(false);

}

void TCPConnection::connect() {
  // cerr<<"connect\n";
  // send a segment with ack
  fill_and_update(true);
 
}

TCPConnection::~TCPConnection() {
    try {
        if (_active) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
	    rst_happen(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

// fill the window and  update the state
void TCPConnection::fill_and_update(bool send_syn)
{
   // only when sending a syn or  _receiver has received a syn,can we fill the window	
   if(send_syn || !_receiver.in_listen()) _sender.fill_window();
  //if there are segemnts to be sent and receiver's ackno is not null
  if(!_sender.segments_out().empty() && _receiver.ackno().has_value())
   {
     int size=_sender.segments_out().size();
     TCPSegment seg=_sender.segments_out().front();	
     WrappingInt32 value=_receiver.ackno().value();
     uint16_t win_size=_receiver.window_size()>numeric_limits<char16_t>::max()?numeric_limits<char16_t>::max() :_receiver.window_size();
     // if ackno han window size is the latest,no need to update
     if(! (seg.header().ack && seg.header().ackno==value && seg.header().win==win_size) ) {
     for(int i=0;i<size;i++)
     {      
     seg=_sender.segments_out().front();	     
     seg.header().ack=true;	     
     seg.header().ackno=value;
      
     //header().win is uint_16 but _receiver.window_size() is size_t
     //so the biggest window size is numeric_limits<char16_t>
     seg.header().win=win_size;
     _sender.segments_out().push(seg);
     _sender.segments_out().pop();
      }
     }
   }
   
   // check whether clean shutdon is needed
   clean_shutdown();

}

// the entire connection should be aborted,deal with rst 
void TCPConnection::rst_happen(bool is_sending)
{  
   _active=false;	
   _sender.stream_in().set_error();
   _receiver.stream_out().set_error();
   if(is_sending) // if sending a rst
   {
      //clear all outbound segements	   
      while(!_sender.segments_out().empty()) _sender.segments_out().pop();
      //generate an empty segemnt
      _sender.send_empty_segment();
      // set rst
      _sender.segments_out().front().header().rst=true;
   }
}

void TCPConnection::clean_shutdown()
{
   // fin received from remote peer before TCPConnection has reached EOF on its outbound stream  ,no need to linger after stream finish,so set it to false
   if(_receiver.in_fin_recv() && !_sender.stream_in().input_ended())
   {
       _linger_after_streams_finish=false;
   }
   if(_sender.in_fin_acked() && _time_since_last_segment_received>=10*_cfg.rt_timeout) _active=false;
   if(!_linger_after_streams_finish && _sender.in_fin_acked() ) _active=false;
}

std::string TCPConnection::sender_state()
{
   if(_sender.in_closed()) return "sender closed";
   if(_sender.in_syn_sent()) return "sender syn_sent";
   if(_sender.in_syn_acked()) return "sender syn_acked";
   if(_sender.in_fin_sent()) return "sender fin_sent";
   if(_sender.in_fin_acked()) return "sender fin_acked";
   return "sender error";
}
std::string TCPConnection::receiver_state()
{
  if(_receiver.in_listen()) return "receiver listen";
  if(_receiver.in_syn_recv()) return "recever syn_recv";
  if(_receiver.in_fin_recv()) return "receiver fin_recv";
  return "receiver error";
}
