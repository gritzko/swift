/*
 *  p2tp.h
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *	
 */
/*	

The P2TP protocol
 
Messages
 
 HANDSHAKE	00, channelid
 Communicates the channel id of the sender. The
 initial handshake packet also has the root hash
 (a HASH message).
 
 DATA		01, bin, buffer
 1K of data.
 
 ACK		02, bin
 Confirms successfull delivery of data. Used for
 congestion control, as well.
 
 HINT		03, bin
 Practical value of "hints" is to avoid overlap, mostly.
 Hints might be lost in the network or ignored.
 Peer might send out data without a hint.
 Hint which was not responded (by DATA) in some RTTs
 is considered to be ignored.
 As peers cant pick randomly kilobyte here and there,
 they send out "long hints" for non-base bins.
 
 HASH		04, bin, sha1hash
 SHA1 hash tree hashes for data verification. The
 connection to a fresh peer starts with bootstrapping
 him with peak hashes. Later, before sending out
 any data, a peer sends the necessary uncle hashes.
 
 PEX+/PEX-	05/06, ip, port
 Peer exchange messages; reports all connected and
 disconected peers. Might has special meaning (as
 in the case with swarm supervisors).
 
 --8X---- maybe (give2get)
 
 CRED		07, scrchid
 Communicated the public channel id at the sender
 side. Any grandchildren's credits will go to this
 channel.
 
 CRED1/2	08/09, ip, port, scrchid
 Second-step and trird-step credits.
 
 ACK1/2		10/11, scrchid, packets
 Grandchildren's acknowledgements of data being
 received from a child; the public channel id
 is mentioned.
 
*/
#ifndef P2TP_H
#define P2TP_H
#include <stdint.h>
#include <iostream>
#include <vector>
#include <deque>
#include "bin.h"
#include "sbit.h"
#include "datagram.h"
#include "hashtree.h"

namespace p2tp {

	/* 64-bit time counter, microseconds since epoch
	typedef int64_t tint;
	static const tint TINT_1SEC = 1000000;
    static const tint TINT_1MSEC = 1000;
	static const tint TINT_INFINITY = 0x7fffffffffffffffULL;
    */
	struct tintbin {
		tint time;
		bin pos;
		tintbin(tint t, bin p) : time(t), pos(p) {}
	};
	typedef std::deque<tintbin> tbinvec;

	typedef enum { 
		P2TP_HANDSHAKE = 0, 
		P2TP_DATA = 1, 
		P2TP_ACK = 2, 
		P2TP_HINT = 3, 
		P2TP_HASH = 4, 
		P2TP_PEX_ADD = 5,
		P2TP_PEX_RM = 6,
		P2TP_MESSAGE_COUNT = 7
	} messageid_t;

	
	struct	File {
		
		typedef enum {EMPTY,IN_PROGRESS,DONE} status_t;

		static std::vector<File*> files;
		
		/**	File descriptor. */
		int				fd;
		/**	Whether the file is completely downloaded. */
		status_t		status_;
		
		//	A map for all packets obtained and succesfully checked.
		bins			ack_out;
		//	Hinted packets.
		bins			hint_out;
		
		HashTree		hashes;
		//	History of bin retrieval.
		bin::vec		history;
		
		// TBD
		uint64_t		options;

		/**	Submit a fresh file. */
		File (int fd);
		/**	Retrieve a file.	*/
		File (Sha1Hash hash, int fd);
		/**	Placeholder. */
		File () : fd(-1), hashes(Sha1Hash::ZERO) {}
		/**	Close everything. */
		~File();
		
		bool OfferHash (bin pos, const Sha1Hash& hash);
		const Sha1Hash& root_hash () const { return hashes.root; }
		size_t		size () const { return hashes.data_size()<<10; }
		size_t		packet_size () const { return hashes.data_size(); }
		status_t status () const { return status_; }
		
		static File* find (const Sha1Hash& hash);
		static File* file (int fd) { return fd<files.size() ? files[fd] : NULL; }

		friend int Open (const char* filename);
		friend int Open (const Sha1Hash& hash, const char* filename);
		friend void Close (int fdes);
		friend class Channel;
	};
	
	
	int		Open (const char* filename) ;
	int		Open (const Sha1Hash& root_hash, const char* filename);	
	size_t	file_size (int fd);	
	void	Close (int fid) ;
	

	struct CongestionControl {
		typedef enum {SLOW_START_STATE,CONG_AVOID_STATE} stdtcpstate_t;
		typedef enum { DATA_EV, ACK_EV, LOSS_EV } CongCtrlEvents;
		
		stdtcpstate_t state_;
		tint rtt_avg_, dev_avg_, last_arrival_, rate_;
		int cwnd_, cainc_, ssthresh_;
		int peer_cwnd_, data_ins_;
		
		CongestionControl();
		virtual ~CongestionControl() {}
		virtual void	OnCongestionEvent (CongCtrlEvents ev);
		void	RttSample (tint rtt);
		
		tint	avg_rtt() const { return rtt_avg_; }
		tint	safe_avg_rtt() const { return avg_rtt() + avg_rtt_dev()*8; }
		tint	avg_rtt_dev() const { return dev_avg_; }
		int		cwnd () const {  return cwnd_; }
		int		peer_cwnd () const;
		int		peer_bps () const;
		tint	data_in_rate () const { return rate_; }
	};
	
	

	/**	P2TP "control block". */
	class Channel {

	public:
		Channel	(int filedes, int socket, struct sockaddr_in peer,
				 uint32_t peer_channel, uint64_t supports=0);
		~Channel();
		
		static void	Recv (int socket);
		void		Recv (Datagram& dgram);
		void		Send ();
		void		SendSomething ();
		void		SendHandshake ();
		void		Tick ();

		typedef enum {HS_REQ_OUT,HS_RES_OUT,HS_DONE} state_t;
		File&		file () { return *File::files[fd]; }
		
		
	
		void		OnAck (Datagram& dgram);
		void		OnData (Datagram& dgram);
		void		OnHint (Datagram& dgram);
		void		OnHash (Datagram& dgram);
		
		void		AddHandshake (Datagram& dgram);
		bin 		AddData (Datagram& dgram);
		void		AddAck (Datagram& dgram);
		void		AddHint (Datagram& dgram);
		void		AddUncleHashes (Datagram& dgram, bin pos);
		void		AddPeakHashes (Datagram& dgram);

		bin			SenderPiecePick () ;
		bin			ReceiverPiecePick (int sizelim) ;
		void		CleanStaleDataOut(bin ack_pos);
		void		CleanStaleHintOut();
		void		CleanStaleHintIn();
		
		state_t		state () const;
		File::status_t	peer_status() const { return peer_status_; }
		const CongestionControl& congestion_control() const { return cc_; }

		static int DecodeID(int scrambled);
		static int EncodeID(int unscrambled);
		static void Loop (tint time);
		static Channel* channel(int ind) {return ind<channels.size()?channels[ind]:NULL;}
		static int MAX_REORDERING;
		static tint TIMEOUT;
		static std::vector<Channel*> channels;
		static int* sockets_;
		static int sock_count_;
		static tint last_tick;
		//static int socket;
		
		friend int Connect (int fd, int sock, 
							const struct sockaddr_in& addr, uint32_t peerch=0);
		friend int Init (int portno);
		friend std::ostream& operator << (std::ostream& os, const Channel& s);
		
	private:
		
		// index in the channel array
		int			id;
		//	Socket address of the peer.
		struct sockaddr_in	peer;
		//	The UDP socket fd.
		int			socket_;
		File::status_t	peer_status_;
		//	Descriptor of the file in question
		int			fd;
		//	Zero if we are trying to open a channel.
		uint32_t	peer_channel_id;
		//	Temporarily, a pointer to a composed/parsed datagram.
		//Datagram*	dgram;
		
		//	Peer's retrieved pieces.
		bins		ack_in;
		//	Progress of piece acknowledgement to the peer.
		bin			data_in_;
		int			ack_out;
		//	Transmit schedule: in most cases filled with the peer's hints
		tbinvec		hint_in;
		//  Hints sent (to detect and reschedule ignored hints).
		tbinvec		hint_out;
		//	Uncle hash send pos; in the case both data and hashes do not fit
		//	into a single datagram, we set this.
		bin			hash_out;
		//	Send history: all cwnd pieces.
		tbinvec		data_out; 
		
		CongestionControl	cc_;
		tint		last_send_time;
	};

	
	//int		connect (int fd, const struct sockaddr_in& addr, uint32_t peerch=0) ;
	void	Loop (tint time=0) ;
	int		Init (int portno) ;
	void	Shutdown (int portno);
	
	uint32_t Width (const tbinvec& v);
}

#define RETLOG(str) { LOG(WARNING)<<str; return; }

#endif
