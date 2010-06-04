/*
 *  swift.h
 *  the main header file for libswift, normally you should only read this one
 *
 *  Created by Victor Grishchenko on 3/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
/*

The swift protocol

Messages

 HANDSHAKE    00, channelid
 Communicates the channel id of the sender. The
 initial handshake packet also has the root hash
 (a HASH message).

 DATA        01, bin_32, buffer
 1K of data.

 ACK        02, bin_32, timestamp_32
 HAVE       03, bin_32
 Confirms successfull delivery of data. Used for
 congestion control, as well.

 HINT        08, bin_32
 Practical value of "hints" is to avoid overlap, mostly.
 Hints might be lost in the network or ignored.
 Peer might send out data without a hint.
 Hint which was not responded (by DATA) in some RTTs
 is considered to be ignored.
 As peers cant pick randomly kilobyte here and there,
 they send out "long hints" for non-base bins.

 HASH        04, bin_32, sha1hash
 SHA1 hash tree hashes for data verification. The
 connection to a fresh peer starts with bootstrapping
 him with peak hashes. Later, before sending out
 any data, a peer sends the necessary uncle hashes.

 PEX+/PEX-    05/06, ipv4 addr, port
 Peer exchange messages; reports all connected and
 disconected peers. Might has special meaning (as
 in the case with swarm supervisors).

*/
#ifndef SWIFT_H
#define SWIFT_H

#include <deque>
#include <vector>
#include <algorithm>
#include <string>
#include "bin64.h"
#include "bins.h"
#include "datagram.h"
#include "hashtree.h"

namespace swift {

    #define NOW Datagram::now

    /** tintbin is basically a pair<tint,bin64_t> plus some nice operators.
        Most frequently used in different queues (acknowledgements, requests,
        etc). */
    struct tintbin {
        tint    time;
        bin64_t bin;
        tintbin(const tintbin& b) : time(b.time), bin(b.bin) {}
        tintbin() : time(TINT_NEVER), bin(bin64_t::NONE) {}
        tintbin(tint time_, bin64_t bin_) : time(time_), bin(bin_) {}
        tintbin(bin64_t bin_) : time(NOW), bin(bin_) {}
        bool operator < (const tintbin& b) const
            { return time > b.time; }
        bool operator == (const tintbin& b) const
            { return time==b.time && bin==b.bin; }
        bool operator != (const tintbin& b) const
            { return !(*this==b); }
    };

    typedef std::deque<tintbin> tbqueue;
    typedef std::deque<bin64_t> binqueue;
    typedef Address   Address;

    /** A heap (priority queue) for timestamped bin numbers (tintbins). */
    class tbheap {
        tbqueue data_;
    public:
        int size () const { return data_.size(); }
        bool is_empty () const { return data_.empty(); }
        tintbin         pop() {
            tintbin ret = data_.front();
            std::pop_heap(data_.begin(),data_.end());
            data_.pop_back();
            return ret;
        }
        void            push(const tintbin& tb) {
            data_.push_back(tb);
            push_heap(data_.begin(),data_.end());
        }
        const tintbin&  peek() const {
            return data_.front();
        }
    };

    /** swift protocol message types; these are used on the wire. */
    typedef enum {
        SWIFT_HANDSHAKE = 0,
        SWIFT_DATA = 1,
        SWIFT_ACK = 2,
        SWIFT_HAVE = 3,
        SWIFT_HASH = 4,
        SWIFT_PEX_ADD = 5,
        SWIFT_PEX_RM = 6,
        SWIFT_SIGNED_HASH = 7,
        SWIFT_HINT = 8,
        SWIFT_MSGTYPE_RCVD = 9,
        SWIFT_MESSAGE_COUNT = 10
    } messageid_t;

    class PiecePicker;
    class CongestionController;
    class PeerSelector;
    typedef void (*ProgressCallback) (int transfer, bin64_t bin);


    /** A class representing single file transfer. */
    class    FileTransfer {

    public:

        /** A constructor. Open/submit/retrieve a file.
         *  @param file_name    the name of the file
         *  @param root_hash    the root hash of the file; zero hash if the file
                                is newly submitted */
        FileTransfer(const char *file_name, const Sha1Hash& root_hash=Sha1Hash::ZERO);

        /**    Close everything. */
        ~FileTransfer();


        /** While we need to feed ACKs to every peer, we try (1) avoid
            unnecessary duplication and (2) keep minimum state. Thus,
            we use a rotating queue of bin completion events. */
        //bin64_t         RevealAck (uint64_t& offset);
        /** Rotating queue read for channels of this transmission. */
        int             RevealChannel (int& i);

        /** Find transfer by the root hash. */
        static FileTransfer* Find (const Sha1Hash& hash);
        /** Find transfer by the file descriptor. */
        static FileTransfer* file (int fd) {
            return fd<files.size() ? files[fd] : NULL;
        }

        /** The binmap for data already retrieved and checked. */
        binmap_t&           ack_out ()  { return file_.ack_out(); }
        /** Piece picking strategy used by this transfer. */
        PiecePicker&    picker () { return *picker_; }
        /** The number of channels working for this transfer. */
        int             channel_count () const { return hs_in_.size(); }
        /** Hash tree checked file; all the hashes and data are kept here. */
        HashTree&       file() { return file_; }
        /** File descriptor for the data file. */
        int             fd () const { return file_.file_descriptor(); }
        /** Root SHA1 hash of the transfer (and the data file). */
        const Sha1Hash& root_hash () const { return file_.root_hash(); }

    private:

        static std::vector<FileTransfer*> files;

        HashTree        file_;

        /** Piece picker strategy. */
        PiecePicker*    picker_;

        /** Channels working for this transfer. */
        binqueue        hs_in_;
        int             hs_in_offset_;
        std::deque<Address> pex_in_;

        /** Messages we are accepting.    */
        uint64_t        cap_out_;

        tint            init_time_;

        #define SWFT_MAX_TRANSFER_CB 8
        ProgressCallback callbacks[SWFT_MAX_TRANSFER_CB];
        uint8_t         cb_agg[SWFT_MAX_TRANSFER_CB];
        int             cb_installed;

    public:
        void            OnDataIn (bin64_t pos);
        void            OnPexIn (const Address& addr);

        friend class Channel;
        friend uint64_t  Size (int fdes);
        friend bool      IsComplete (int fdes);
        friend uint64_t  Complete (int fdes);
        friend uint64_t  SeqComplete (int fdes);
        friend int     Open (const char* filename, const Sha1Hash& hash) ;
        friend void    Close (int fd) ;
        friend void AddProgressCallback (int transfer,ProgressCallback cb,uint8_t agg);
        friend void RemoveProgressCallback (int transfer,ProgressCallback cb);
        friend void ExternallyRetrieved (int transfer,bin64_t piece);
    };


    /** PiecePicker implements some strategy of choosing (picking) what
        to request next, given the possible range of choices:
        data acknowledged by the peer minus data already retrieved.
        May pick sequentially, do rarest first or in some other way. */
    class PiecePicker {
    public:
        virtual void Randomize (uint64_t twist) = 0;
        /** The piece picking method itself.
         *  @param  offered     the daata acknowledged by the peer
         *  @param  max_width   maximum number of packets to ask for
         *  @param  expires     (not used currently) when to consider request expired
         *  @return             the bin number to request */
        virtual bin64_t Pick (binmap_t& offered, uint64_t max_width, tint expires) = 0;
        virtual void LimitRange (bin64_t range) = 0;
        virtual ~PiecePicker() {}
    };


    class PeerSelector {
    public:
        virtual void AddPeer (const Address& addr, const Sha1Hash& root) = 0;
        virtual Address GetPeer (const Sha1Hash& for_root) = 0;
    };


    class DataStorer {
    public:
        DataStorer (const Sha1Hash& id, size_t size);
        virtual size_t    ReadData (bin64_t pos,uint8_t** buf) = 0;
        virtual size_t    WriteData (bin64_t pos, uint8_t* buf, size_t len) = 0;
    };


    /**    swift channel's "control block"; channels loosely correspond to TCP
        connections or FTP sessions; one channel is created for one file
        being transferred between two peers. As we don't need buffers and
        lots of other TCP stuff, sizeof(Channel+members) must be below 1K.
        Normally, API users do not deal with this class. */
    class Channel {
    public:
        Channel    (FileTransfer* file, int socket=INVALID_SOCKET, Address peer=Address());
        ~Channel();

        typedef enum {
            KEEP_ALIVE_CONTROL,
            PING_PONG_CONTROL,
            SLOW_START_CONTROL,
            AIMD_CONTROL,
            LEDBAT_CONTROL,
            CLOSE_CONTROL
        } send_control_t;

        static const char* SEND_CONTROL_MODES[];

        static void RecvDatagram (SOCKET socket);
        static void Loop (tint till);

        void        Recv (Datagram& dgram);
        void        Send ();
        void        Close ();

        void        OnAck (Datagram& dgram);
        void        OnHave (Datagram& dgram);
        bin64_t     OnData (Datagram& dgram);
        void        OnHint (Datagram& dgram);
        void        OnHash (Datagram& dgram);
        void        OnPex (Datagram& dgram);
        void        OnHandshake (Datagram& dgram);
        void        AddHandshake (Datagram& dgram);
        bin64_t     AddData (Datagram& dgram);
        void        AddAck (Datagram& dgram);
        void        AddHave (Datagram& dgram);
        void        AddHint (Datagram& dgram);
        void        AddUncleHashes (Datagram& dgram, bin64_t pos);
        void        AddPeakHashes (Datagram& dgram);
        void        AddPex (Datagram& dgram);

        void        BackOffOnLosses (float ratio=0.5);
        tint        SwitchSendControl (int control_mode);
        tint        NextSendTime ();
        tint        KeepAliveNextSendTime ();
        tint        PingPongNextSendTime ();
        tint        CwndRateNextSendTime ();
        tint        SlowStartNextSendTime ();
        tint        AimdNextSendTime ();
        tint        LedbatNextSendTime ();

        static int  MAX_REORDERING;
        static tint TIMEOUT;
        static tint MIN_DEV;
        static tint MAX_SEND_INTERVAL;
        static tint LEDBAT_TARGET;
        static float LEDBAT_GAIN;
        static tint LEDBAT_DELAY_BIN;
        static bool SELF_CONN_OK;
        static tint MAX_POSSIBLE_RTT;
        static FILE* debug_file;

        const std::string id_string () const;
        /** A channel is "established" if had already sent and received packets. */
        bool        is_established () { return peer_channel_id_ && own_id_mentioned_; }
        FileTransfer& transfer() { return *transfer_; }
        HashTree&   file () { return transfer_->file(); }
        const Address& peer() const { return peer_; }
        tint ack_timeout () {
			tint dev = dev_avg_ < MIN_DEV ? MIN_DEV : dev_avg_;
			tint tmo = rtt_avg_ + dev * 4;
			return tmo < 30*TINT_SEC ? tmo : 30*TINT_SEC;
        }
        uint32_t    id () const { return id_; }

        static int  DecodeID(int scrambled);
        static int  EncodeID(int unscrambled);
        static Channel* channel(int i) {
            return i<channels.size()?channels[i]:NULL;
        }
        static void CloseTransfer (FileTransfer* trans);

    protected:
        /** Channel id: index in the channel array. */
        uint32_t    id_;
        /**    Socket address of the peer. */
        Address     peer_;
        /**    The UDP socket fd. */
        SOCKET      socket_;
        /**    Descriptor of the file in question. */
        FileTransfer*    transfer_;
        /**    Peer channel id; zero if we are trying to open a channel. */
        uint32_t    peer_channel_id_;
        bool        own_id_mentioned_;
        /**    Peer's progress, based on acknowledgements. */
        binmap_t        ack_in_;
        /**    Last data received; needs to be acked immediately. */
        tintbin     data_in_;
        bin64_t     data_in_dbl_;
        /** The history of data sent and still unacknowledged. */
        tbqueue     data_out_;
        /** Timeouted data (potentially to be retransmitted). */
        tbqueue     data_out_tmo_;
        bin64_t     data_out_cap_;
        /** Index in the history array. */
        binmap_t        have_out_;
        /**    Transmit schedule: in most cases filled with the peer's hints */
        tbqueue     hint_in_;
        /** Hints sent (to detect and reschedule ignored hints). */
        tbqueue     hint_out_;
        uint64_t    hint_out_size_;
        /** Types of messages the peer accepts. */
        uint64_t    cap_in_;
        /** For repeats. */
        //tint        last_send_time, last_recv_time;
        /** PEX progress */
        int         pex_out_;
        /** Smoothed averages for RTT, RTT deviation and data interarrival periods. */
        tint        rtt_avg_, dev_avg_, dip_avg_;
        tint        last_send_time_;
        tint        last_recv_time_;
        tint        last_data_out_time_;
        tint        last_data_in_time_;
        tint        last_loss_time_;
        tint        next_send_time_;
        /** Congestion window; TODO: int, bytes. */
        float       cwnd_;
        /** Data sending interval. */
        tint        send_interval_;
        /** The congestion control strategy. */
        int         send_control_;
        /** Datagrams (not data) sent since last recv.    */
        int         sent_since_recv_;
        /** Recent acknowlegements for data previously sent.    */
        int         ack_rcvd_recent_;
        /** Recent non-acknowlegements (losses) of data previously sent.    */
        int         ack_not_rcvd_recent_;
        /** LEDBAT one-way delay machinery */
        tint        owd_min_bins_[4];
        int         owd_min_bin_;
        tint        owd_min_bin_start_;
        tint        owd_current_[4];
        int         owd_cur_bin_;
        /** Stats */
        int         dgrams_sent_;
        int         dgrams_rcvd_;

        int         PeerBPS() const {
            return TINT_SEC / dip_avg_ * 1024;
        }
        /** Get a request for one packet from the queue of peer's requests. */
        bin64_t     DequeueHint();
        bin64_t     ImposeHint();
        void        TimeoutDataOut ();
        void        CleanStaleHintOut();
        void        CleanHintOut(bin64_t pos);
        void        Reschedule();

        static PeerSelector* peer_selector;

        static tint     last_tick;
        static tbheap   send_queue;

        static Address  tracker;
        static std::vector<Channel*> channels;

        friend int      Listen (Address addr);
        friend void     Shutdown (int sock_des);
        friend void     AddPeer (Address address, const Sha1Hash& root);
        friend void     SetTracker(const Address& tracker);
        friend int      Open (const char*, const Sha1Hash&) ; // FIXME

    };



    /*************** The top-level API ****************/
    /** Start listening a port. Returns socket descriptor. */
    int     Listen (Address addr);
    /** Run send/receive loop for the specified amount of time. */
    void    Loop (tint till);
    bool    Listen3rdPartySocket (sckrwecb_t);
    /** Stop listening to a port. */
    void    Shutdown (int sock_des=-1);

    /** Open a file, start a transmission; fill it with content for a given root hash;
        in case the hash is omitted, the file is a fresh submit. */
    int     Open (const char* filename, const Sha1Hash& hash=Sha1Hash::ZERO) ;
    /** Get the root hash for the transmission. */
    const Sha1Hash& RootMerkleHash (int file) ;
    /** Close a file and a transmission. */
    void    Close (int fd) ;
    /** Add a possible peer which participares in a given transmission. In the case
        root hash is zero, the peer might be talked to regarding any transmission
        (likely, a tracker, cache or an archive). */
    void    AddPeer (Address address, const Sha1Hash& root=Sha1Hash::ZERO);

    void    SetTracker(const Address& tracker);

    /** Returns size of the file in bytes, 0 if unknown. Might be rounded up to a kilobyte
        before the transmission is complete. */
    uint64_t  Size (int fdes);
    /** Returns the amount of retrieved and verified data, in bytes.
        A 100% complete transmission has Size()==Complete(). */
    uint64_t  Complete (int fdes);
    bool      IsComplete (int fdes);
    /** Returns the number of bytes that are complete sequentially, starting from the
        beginning, till the first not-yet-retrieved packet. */
    uint64_t  SeqComplete (int fdes);
    /***/
    int       Find (Sha1Hash hash);

    void AddProgressCallback (int transfer,ProgressCallback cb,uint8_t agg);
    void RemoveProgressCallback (int transfer,ProgressCallback cb);
    void ExternallyRetrieved (int transfer,bin64_t piece);

    //uint32_t Width (const tbinvec& v);


    /** Must be called by any client using the library */
    void LibraryInit(void);


} // namespace end

#ifndef SWIFT_MUTE
#define dprintf(...) { if (Channel::debug_file) fprintf(Channel::debug_file,__VA_ARGS__); }
#else
#define dprintf(...) {}
#endif
#define eprintf(...) fprintf(stderr,__VA_ARGS__)

#endif
