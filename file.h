#include <mmap.h>

namespace p2tp {

	class	File {
		
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
	
	
    	int		Open (const char* filename) ;
	    int		Open (const Sha1Hash& root_hash, const char* filename);	
    	size_t	file_size (int fd);	
    	void	Close (int fid) ;
    	
        void*   Map(bin64_t bin);
        void    UnMap(void*);

	};

};
