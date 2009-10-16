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
 
 DATA		01, bin_32, buffer
 1K of data.
 
 ACK		02, bin_32
 ACKTS      08, bin_32, timestamp_32
 Confirms successfull delivery of data. Used for
 congestion control, as well.
  
 HINT		03, bin_32
 Practical value of "hints" is to avoid overlap, mostly.
 Hints might be lost in the network or ignored.
 Peer might send out data without a hint.
 Hint which was not responded (by DATA) in some RTTs
 is considered to be ignored.
 As peers cant pick randomly kilobyte here and there,
 they send out "long hints" for non-base bins.
 
 HASH		04, bin_32, sha1hash
 SHA1 hash tree hashes for data verification. The
 connection to a fresh peer starts with bootstrapping
 him with peak hashes. Later, before sending out
 any data, a peer sends the necessary uncle hashes.
 
 PEX+/PEX-	05/06, ipv4 addr, port
 Peer exchange messages; reports all connected and
 disconected peers. Might has special meaning (as
 in the case with swarm supervisors).
 
*/
#ifndef P2TP_H
#define P2TP_H
#include <stdint.h>
#include <vector>
#include <deque>
#include "bin64.h"
#include "bins.h"
#include "datagram.h"
#include "hashtree.h"

namespace p2tp {

    struct tintbin {
        tint    time;
        bin64_t bin;
        tintbin(const tintbin& b) : time(b.time), bin(b.bin) {}
        tintbin() : time(0), bin(bin64_t::NONE) {}
        tintbin(tint time_, bin64_t bin_) : time(time_), bin(bin_) {}
    };
    
	typedef std::deque<tintbin> tbqueue;
    typedef std::deque<bin64_t> binqueue;

	typedef enum { 
		P2TP_HANDSHAKE = 0, 
		P2TP_DATA = 1, 
		P2TP_ACK = 2, 
		P2TP_ACK_TS = 8, 
		P2TP_HINT = 3, 
		P2TP_HASH = 4, 
		P2TP_PEX_ADD = 5,
		P2TP_PEX_RM = 6,
		P2TP_MESSAGE_COUNT = 7
	} messageid_t;
    
    class PiecePicker;
    class CongestionController;
    class PeerSelector;

	
	class	FileTransfer {
		
    public:
		
		/**	Open/submit/retrieve a file.	*/
        FileTransfer(const Sha1Hash& _root_hash, const char *file_name);
        
		/**	Close everything. */
		~FileTransfer();
        
        /** Offer a hash; returns true if it verified; false otherwise.
         Once it cannot be verified (no sibling or parent), the hash
         is remembered, while returning false. */
		void            OfferHash (bin64_t pos, const Sha1Hash& hash);
        /** Offer data; the behavior is the same as with a hash:
            accept or remember or drop. Returns true => ACK is sent. */
        bool            OfferData (bin64_t bin, uint8_t* data, size_t length);
        
		static FileTransfer* Find (const Sha1Hash& hash);
		static FileTransfer* file (int fd) { 
            return fd<files.size() ? files[fd] : NULL; 
        }
        
        int             GetPeakCount () const { return peak_count; }
        bin64_t         GetPeak (int i) const { return peaks[i]; }
        const Sha1Hash& GetPeakHash (int i) const { return peak_hashes[i]; }
        bin64_t         GetPeakFor (bin64_t pos) const;
        const Sha1Hash& GetHash (bin64_t pos) const { 
            assert(pos<sizek*2);
            return hashes[pos];
        }
        const Sha1Hash& GetRootHash () const { return root_hash; }
        
        
		friend int      Open (const char* filename);
		friend int      Open (const Sha1Hash& hash, const char* filename);
		friend void     Close (int fdes);
        
    public:

		static std::vector<FileTransfer*> files;
        static const char* HASH_FILE_TEMPLATE;
        static const char* PEAK_FILE_TEMPLATE;
        static int instance;
        		
		/**	file descriptor. */
		int				fd;
        /** File size, as derived from the hashes. */
        size_t          size;
        size_t          sizek;
		/**	Part the file currently downloaded. */
		size_t          complete;
		size_t          completek;
		size_t          seq_complete;
		/**	A map for all packets obtained and succesfully checked. */
		bins			ack_out;
		/**	History of bin retrieval. */
		binqueue		data_in;
        /** Piece picker strategy. */
        PiecePicker*    picker;
		/** File for keeping the Merkle hash tree. */
        int             hashfd;
        /** Merkle hash tree: root */
        Sha1Hash        root_hash;
        /** Merkle hash tree: peak hashes */
        Sha1Hash        peak_hashes[64];
        bin64_t         peaks[64];
        int             peak_count;
        /** Merkle hash tree: the tree, as a bin64_t-indexed array */
        Sha1Hash*       hashes;
        /** for recovering saved state */
        bool            dry_run;
        /** Error encountered */
        char*           error;
        
    protected:
        void            SetSize(size_t bytes);
        void            Submit();
        void            RecoverProgress();
        void            OfferPeak (bin64_t pos, const Sha1Hash& hash);
        Sha1Hash        DeriveRoot();
        void            SavePeaks();
        void            LoadPeaks();

		friend class Channel;
	};
	
	
	int		Open (const char* filename) ;
    int     Open (const Sha1Hash& hash, const char* filename) ;
	void	Close (int fid) ;
    void    Loop (tint till);
    int     Bind (int port);
    void    Shutdown (int fd);
    void    HeardOfPeer (const Sha1Hash& root, struct sockaddr_in address);
	

	class CongestionController {
    public:
        tint    rtt_avg;
        tint    dev_avg;
        int     cwnd;
        int     peer_cwnd;
        virtual void    OnDataSent(bin64_t b) = 0;
        virtual void    OnDataRecvd(bin64_t b) = 0;
        virtual void    OnAckRcvd(const tintbin& tsack) = 0;
		virtual         ~CongestionController() = 0;
	};
    
    class PiecePicker {
    public:
        virtual bin64_t Pick (bins& from, uint8_t layer) = 0;
        virtual void    Received (bin64_t b) = 0;
        virtual void    Snubbed (bin64_t b) = 0;
    };
    
    class PeerSelector {
    public:
        virtual void    PeerKnown 
            (const Sha1Hash& root, struct sockaddr_in& addr) = 0;
        virtual struct sockaddr_in GetPeer 
            (const Sha1Hash& for_root) = 0;
    };
    
    class DataStorer {
    public:
        DataStorer (const Sha1Hash& id, size_t size);
        virtual size_t    ReadData (bin64_t pos,uint8_t** buf) = 0;
        virtual size_t    WriteData (bin64_t pos, uint8_t* buf, size_t len) = 0;
    };
	

	/**	P2TP channel's "control block"; channels loosely correspond to TCP
        connections or FTP sessions; one channel is created for one file
        being transferred between two peers. As we don't need buffers and
        lots of other TCP stuff, sizeof(Channel+members) must be below 1K.
        (There was a seductive idea to remove channels, just put the root
        hash or a fragment of it into every datagram.) */
	class Channel {
	public:
		Channel	(int filedes, int socket, struct sockaddr_in peer,
				 uint32_t peer_channel, uint64_t supports=0);
		~Channel();
		
		static void	Recv (int socket);
		void		Recv (Datagram& dgram);
		tint		Send ();

		void		OnAck (Datagram& dgram);
		void		OnAckTs (Datagram& dgram);
		void		OnData (Datagram& dgram);
		void		OnHint (Datagram& dgram);
		void		OnHash (Datagram& dgram);
		void		OnPex (Datagram& dgram);
		
		void		AddHandshake (Datagram& dgram);
		bin64_t		AddData (Datagram& dgram);
		void		AddAck (Datagram& dgram);
		void		AddHint (Datagram& dgram);
		void		AddUncleHashes (Datagram& dgram, bin64_t pos);
		void		AddPeakHashes (Datagram& dgram);

        const std::string id_string () const;
        /** A channel is "established" if had already sent and received packets. */
        bool        is_established () { return peer_channel_id && own_id_mentioned; }
        
		static int DecodeID(int scrambled);
		static int EncodeID(int unscrambled);
		static Channel* channel(int i) {
            return i<channels.size()?channels[i]:NULL;
        }
        
        FileTransfer& file() { return *file_; }
		
		/*friend int Connect (int fd, int sock, 
							const struct sockaddr_in& addr, uint32_t peerch=0);
		friend int Init (int portno);
		friend std::ostream& operator << (std::ostream& os, const Channel& s);*/
		
	private:
		
		/** Channel id: index in the channel array. */
		uint32_t	id;
		/**	Socket address of the peer. */
		struct sockaddr_in	peer;
		/**	The UDP socket fd. */
		int			socket;
		/**	Descriptor of the file in question. */
		FileTransfer*	file_;
		/**	Peer channel id; zero if we are trying to open a channel. */
		uint32_t	peer_channel_id;
        bool        own_id_mentioned;
		/**	Peer's progress, based on acknowledgements. */
		bins		ack_in;
		/**	Last data received; needs to be acked immediately. */
		tintbin		data_in_;
        /** Index in the history array. */
		uint32_t	ack_out_;
		/**	Transmit schedule: in most cases filled with the peer's hints */
		binqueue    hint_in;
		/** Hints sent (to detect and reschedule ignored hints). */
		tbqueue		hint_out;
		/** The congestion control strategy. */
		CongestionController	*cc;
        /** For repeats. */
		tint		last_send_time, last_recv_time;
        
        /** Get a rewuest for one packet from the queue of peer's requests. */
        bin64_t		DequeueHint();
        void        CleanStaleHints();
        
        static PeerSelector* peer_selector;
        
		static int  MAX_REORDERING;
		static tint TIMEOUT;
		static std::vector<Channel*> channels;
		static tint last_tick;
        
	};

	
	//uint32_t Width (const tbinvec& v);
}

#define RETLOG(str) { LOG(WARNING)<<str; return; }

#endif
