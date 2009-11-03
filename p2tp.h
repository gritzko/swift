/*
 *  p2tp.h
 *  serp++
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
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

#ifdef _WIN32
#include "compat/stdint.h"
#else
#include <stdint.h>
#endif
#include <vector>
#include <deque>
#include <string>
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
        tintbin(bin64_t bin_) : time(Datagram::now), bin(bin_) {}
    };

    
	typedef std::deque<tintbin> tbqueue;
    typedef std::deque<bin64_t> binqueue;
    typedef Datagram::Address   Address;

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
        FileTransfer(const char *file_name, const Sha1Hash& _root_hash=Sha1Hash::ZERO);

		/**	Close everything. */
		~FileTransfer();

        /** Offer a hash; returns true if it verified; false otherwise.
         Once it cannot be verified (no sibling or parent), the hash
         is remembered, while returning false. */
		void            OfferHash (bin64_t pos, const Sha1Hash& hash);
        /** Offer data; the behavior is the same as with a hash:
            accept or remember or drop. Returns true => ACK is sent. */
        bool            OfferData (bin64_t bin, const uint8_t* data, size_t length);

        /** While we need to feed ACKs to every peer, we try (1) avoid
            unnecessary duplication and (2) keep minimum state. Thus,
            we use a rotating queue of bin completion events. */
        //bin64_t         RevealAck (uint64_t& offset);
        /** Rotating queue read for channels of this transmission. */
        uint32_t        RevealChannel (int& i);
        
        static FileTransfer* Find (const Sha1Hash& hash);
		static FileTransfer* file (int fd) {
            return fd<files.size() ? files[fd] : NULL;
        }

        int             peak_count () const { return peak_count_; }
        bin64_t         peak (int i) const { return peaks_[i]; }
        const Sha1Hash& peak_hash (int i) const { return peak_hashes_[i]; }
        bin64_t         peak_for (bin64_t pos) const;
        const Sha1Hash& hash (bin64_t pos) const {
            assert(pos<sizek_*2);
            return hashes_[pos];
        }
        const Sha1Hash& root_hash () const { return root_hash_; }
        uint64_t        size () const { return size_; }
        uint64_t        size_kilo () const { return sizek_; }
        uint64_t        complete () const { return complete_; }
        uint64_t        complete_kilo () const { return completek_; }
        uint64_t        seq_complete () const { return seq_complete_; }
        bool            is_complete () const
            { return seq_complete_==size_; }
        bins&           ack_out ()  { return ack_out_; }
        int             file_descriptor () const { return fd_; }
        PiecePicker&    picker () { return *picker_; }
        int             channel_count () const { return handshake_in_.size(); }

        static int instance; // FIXME this smells

    private:

		static std::vector<FileTransfer*> files;
		static std::string GetTempFilename(Sha1Hash& root_hash, int instance, std::string postfix);

		/**	file descriptor. */
		int				fd_;
        /** File size, as derived from the hashes. */
        size_t          size_;
        size_t          sizek_;
		/**	Part the file currently downloaded. */
		size_t          complete_;
		size_t          completek_;
		size_t          seq_complete_;
		/**	A binmap for all packets obtained and succesfully checked. */
		bins			ack_out_;
		/**	History of bin retrieval. */
		//binqueue		data_in_;
        //int             data_in_off_;
        /** Piece picker strategy. */
        PiecePicker*    picker_;
		/** File for keeping the Merkle hash tree. */
        int             hashfd_;
#ifdef _WIN32
        HANDLE			hashmaphandle_;
#endif
        /** Merkle hash tree: root */
        Sha1Hash        root_hash_;
        /** Merkle hash tree: peak hashes */
        Sha1Hash        peak_hashes_[64];
        bin64_t         peaks_[64];
        int             peak_count_;
        /** Merkle hash tree: the tree, as a bin64_t-indexed array */
        Sha1Hash*       hashes_;
        /** for recovering saved state */
        bool            dry_run_;
        /** Error encountered */
        char*           error_;

        /** Channels working for this transfer. */
        std::deque<int> handshake_in_;
        std::deque<Address>        pex_in_;
        /** Messages we are accepting.    */
        uint64_t        cap_out_;

    protected:
        void            SetSize(size_t bytes);
        void            Submit();
        void            RecoverProgress();
        void            OfferPeak (bin64_t pos, const Sha1Hash& hash);
        Sha1Hash        DeriveRoot();
        void            SavePeaks();
        void            LoadPeaks();
        void            OnDataIn (bin64_t pos);
        void            OnPexIn (const Address& addr);

		friend class Channel;
        friend size_t  Size (int fdes);
        friend size_t  Complete (int fdes);
        friend size_t  SeqComplete (int fdes);
        friend int     Open (const char* filename, const Sha1Hash& hash) ;
        friend void    Close (int fd) ;
	};

	struct CongestionController {
        int channel_id;
        CongestionController (int chann_id) : channel_id(chann_id) {}
        virtual int     free_cwnd() = 0;
        virtual tint    RoundTripTime() = 0;
        virtual tint    RoundTripTimeoutTime() = 0;
        virtual int     PeerBPS() = 0;
        virtual float   PeerCWindow() = 0;
        virtual tint    OnDataSent(bin64_t b) = 0;
        virtual tint    OnDataRecvd(bin64_t b) = 0;
        virtual tint    OnAckRcvd(bin64_t ackd, tint peer_time=0) = 0;
        //virtual tint    OnHintRecvd (bin64_t hint) = 0;
		virtual         ~CongestionController() {}
	};

    class PiecePicker {
    public:
        virtual bin64_t Pick (bins& offered, uint8_t layer) = 0;
        virtual void    Expired (bin64_t b) = 0;
        virtual void    Received (bin64_t b) = 0;
    };

    class PeerSelector {
    public:
        virtual void AddPeer (const Datagram::Address& addr, const Sha1Hash& root) = 0;
        virtual Datagram::Address GetPeer (const Sha1Hash& for_root) = 0;
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
		Channel	(FileTransfer* file, int socket=-1, Address peer=Address());
		~Channel();

		static void	Recv (int socket);
        static void Loop (tint till);

		void		Recv (Datagram& dgram);
		void		Send ();

		void		OnAck (Datagram& dgram);
		void		OnAckTs (Datagram& dgram);
		bin64_t		OnData (Datagram& dgram);
		void		OnHint (Datagram& dgram);
		void		OnHash (Datagram& dgram);
		void		OnPex (Datagram& dgram);
		void		OnHandshake (Datagram& dgram);
		void		AddHandshake (Datagram& dgram);
		bin64_t		AddData (Datagram& dgram);
		void		AddAck (Datagram& dgram);
		void		AddHint (Datagram& dgram);
		void		AddUncleHashes (Datagram& dgram, bin64_t pos);
		void		AddPeakHashes (Datagram& dgram);
		void		AddPex (Datagram& dgram);

        const std::string id_string () const;
        /** A channel is "established" if had already sent and received packets. */
        bool        is_established () { return peer_channel_id_ && own_id_mentioned_; }
        FileTransfer& file() { return *file_; }
        const Address& peer() const { return peer_; }

		static int DecodeID(int scrambled);
		static int EncodeID(int unscrambled);
		static Channel* channel(int i) {
            return i<channels.size()?channels[i]:NULL;
        }

	private:

		/** Channel id: index in the channel array. */
		uint32_t	id;
		/**	Socket address of the peer. */
        Datagram::Address	peer_;
		/**	The UDP socket fd. */
		int			socket_;
		/**	Descriptor of the file in question. */
		FileTransfer*	file_;
		/**	Peer channel id; zero if we are trying to open a channel. */
		uint32_t	peer_channel_id_;
        bool        own_id_mentioned_;
		/**	Peer's progress, based on acknowledgements. */
		bins		ack_in_;
		/**	Last data received; needs to be acked immediately. */
		tintbin		data_in_;
        /** Index in the history array. */
		bins        ack_out_;
		/**	Transmit schedule: in most cases filled with the peer's hints */
		binqueue    hint_in_;
		/** Hints sent (to detect and reschedule ignored hints). */
		tbqueue		hint_out_;
		/** The congestion control strategy. */
		CongestionController	*cc_;
        /** Types of messages the peer accepts. */
        uint64_t    cap_in_;
        /** For repeats. */
		//tint		last_send_time, last_recv_time;
        /** PEX progress */
        int         pex_out_;

        tint        next_send_time_;
        static      tbqueue send_queue;
        void        RequeueSend (tint next_time);
        
        /** Get a rewuest for one packet from the queue of peer's requests. */
        bin64_t		DequeueHint();
        //void        CleanStaleHints();

        static PeerSelector* peer_selector;

		static int      MAX_REORDERING;
		static tint     TIMEOUT;
        static int      sockets[8];
        static int      socket_count;
		static tint     last_tick;

        static Address  tracker;
		static std::vector<Channel*> channels;

        friend int      Listen (Datagram::Address addr);
        friend void     Shutdown (int sock_des);
        friend void     AddPeer (Datagram::Address address, const Sha1Hash& root);
        friend void     SetTracker(const Address& tracker);
        friend int      Open (const char*, const Sha1Hash&) ; // FIXME
        
        friend class FileTransfer; // FIXME!!!
	};



    /*************** The top-level API ****************/
    /** Start listening a port. Returns socket descriptor. */
    int     Listen (Datagram::Address addr);
    /** Run send/receive loop for the specified amount of time. */
    void    Loop (tint till);
    /** Stop listening to a port. */
    void    Shutdown (int sock_des=-1);

    /** Open a file, start a transmission; fill it with content for a given root hash;
        in case the hash is omitted, the file is a fresh submit. */
    int     Open (const char* filename, const Sha1Hash& hash=Sha1Hash::ZERO) ;
    /** Close a file and a transmission. */
    void	Close (int fd) ;
    /** Add a possible peer which participares in a given transmission. In the case
        root hash is zero, the peer might be talked to regarding any transmission
        (likely, a tracker, cache or an archive). */
    void    AddPeer (Datagram::Address address, const Sha1Hash& root=Sha1Hash::ZERO);
    
    void    SetTracker(const Address& tracker);
    
    /** Returns size of the file in bytes, 0 if unknown. Might be rounded up to a kilobyte
        before the transmission is complete. */
    size_t  Size (int fdes);
    /** Returns the amount of retrieved and verified data, in bytes.
        A 100% complete transmission has Size()==Complete(). */
    size_t  Complete (int fdes);
    /** Returns the number of bytes that are complete sequentially, starting from the
        beginning, till the first not-yet-retrieved packet. */
    size_t  SeqComplete (int fdes);


	//uint32_t Width (const tbinvec& v);


// FIXME kill this macro
#define RETLOG(str) { fprintf(stderr,"%s\n",str); return; }

	/** Must be called by any client using the library */
	void LibraryInit(void);


} // namespace end


#endif
