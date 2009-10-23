/*
 *  transfer.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009 Delft University of Technology. All rights reserved.
 *
 */
#ifdef _WIN32
#include "compat/unixio.h"
#else
#include <sys/mman.h>
#endif
#include <errno.h>
#include <string>
#include <sstream>
#include "p2tp.h"
#include "compat/util.h"

using namespace p2tp;

std::vector<FileTransfer*> FileTransfer::files(20);

int FileTransfer::instance = 0;
#define BINHASHSIZE (sizeof(bin64_t)+sizeof(Sha1Hash))

#include "ext/seq_picker.cpp"

// FIXME: separate Bootstrap() and Download(), then Size(), Progress(), SeqProgress()

FileTransfer::FileTransfer (const char* filename, const Sha1Hash& _root_hash) :
    root_hash_(_root_hash), fd_(0), hashfd_(0), dry_run_(false),
    peak_count_(0), hashes_(NULL), error_(NULL), size_(0), sizek_(0),
    complete_(0), completek_(0), seq_complete_(0)
{
	fd_ = open(filename,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (fd_<0)
        return;
    if (files.size()<fd_+1)
        files.resize(fd_+1);
    files[fd_] = this;
    if (root_hash_==Sha1Hash::ZERO) // fresh submit, hash it
        Submit();
    else
        RecoverProgress();
    picker_ = new SeqPiecePicker(this);
}


void FileTransfer::LoadPeaks () {
    std::string file_name = GetTempFilename(root_hash_,instance,std::string(".peaks"));
    int peakfd = open(file_name.c_str(),O_RDONLY,0);
    if (peakfd<0)
        return;
    bin64_t peak;
    char hash[128];
    while (sizeof(bin64_t)==read(peakfd,&peak,sizeof(bin64_t))) {
        read(peakfd,hash,Sha1Hash::SIZE);
        OfferPeak(peak, Sha1Hash(false,hash));
    }
    close(peakfd);
}


/** Basically, simulated receiving every single packet, except
    for some optimizations. */
void            FileTransfer::RecoverProgress () {
    dry_run_ = true;
    LoadPeaks();
    if (!size())
        return;
    // at this point, we may use mmapd hashes already
    // so, lets verify hashes and the data we've got
    lseek(fd_,0,SEEK_SET);
    for(int p=0; p<size_kilo(); p++) {
        uint8_t buf[1<<10];
        size_t rd = read(fd_,buf,1<<10);
        OfferData(bin64_t(0,p), buf, rd);
        if (rd<(1<<10))
            break;
    }
    dry_run_ = false;
}


void    FileTransfer::SavePeaks () {
    std::string file_name = GetTempFilename(root_hash_,instance,std::string(".peaks"));
    int peakfd = open(file_name.c_str(),O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    for(int i=0; i<peak_count(); i++) {
        write(peakfd,&(peaks_[i]),sizeof(bin64_t));
        write(peakfd,*peak_hashes_[i],Sha1Hash::SIZE);
    }
    close(peakfd);
}


void FileTransfer::SetSize (size_t bytes) { // peaks/root must be already set
    size_ = bytes;
    completek_ = complete_ = seq_complete_ = 0;
	sizek_ = (size_>>10) + ((size_&1023) ? 1 : 0);

	struct stat st;
	fstat(fd_, &st);
    if (st.st_size!=bytes)
        if (ftruncate(fd_, bytes))
            return; // remain in the 0-state
    // mmap the hash file into memory
    std::string file_name = GetTempFilename(root_hash_,instance,std::string(".hashes"));
	hashfd_ = open(file_name.c_str(),O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    size_t expected_size_ = Sha1Hash::SIZE * sizek_ * 2;
	struct stat hash_file_st;
	fstat(hashfd_, &hash_file_st);
    if ( hash_file_st.st_size != expected_size_ )
        ftruncate(hashfd_, expected_size_);
#ifdef _WIN32
    HANDLE hashhandle = (HANDLE)_get_osfhandle(hashfd_);
    hashmaphandle_ = CreateFileMapping(hashhandle,
					      NULL,
					      PAGE_READWRITE,
					      0,
					      0,
					      NULL);
	if (hashmaphandle_ != NULL)
	{
		hashes_ = (Sha1Hash*)MapViewOfFile(hashmaphandle_,
							 FILE_MAP_WRITE,
						     0,
						     0,
						     0);

	}
	if (hashmaphandle_ == NULL || hashes_ == NULL)
#else
    hashes_ = (Sha1Hash*) mmap (NULL, expected_size_, PROT_READ|PROT_WRITE,
                               MAP_SHARED, hashfd_, 0);
    if (hashes_==MAP_FAILED)
#endif
    {
        hashes_ = NULL;
        size_ = sizek_ = complete_ = completek_ = seq_complete_ = 0;
        error_ = strerror(errno); // FIXME dprintf()
        perror("hash tree mmap failed");
        return;
    }
    for(int i=0; i<peak_count_; i++)
        hashes_[peaks_[i]] = peak_hashes_[i];
}


void            FileTransfer::Submit () {
	struct stat st; // TODO:   AppendData()   and   streaming
	fstat(fd_, &st);
    size_ = st.st_size;
	sizek_ = (size_>>10) + ((size_&1023) ? 1 : 0);
    hashes_ = (Sha1Hash*) malloc(Sha1Hash::SIZE*sizek_*2);
    peak_count_ = bin64_t::peaks(sizek_,peaks_);
    for (int p=0; p<peak_count_; p++) {
        for(bin64_t b=peaks_[p].left_foot(); b.within(peaks_[p]); b=b.next_dfsio(0))
            if (b.is_base()) {
                uint8_t kilo[1<<10];
                size_t rd = pread(fd_,kilo,1<<10,b.base_offset()<<10);
                hashes_[b] = Sha1Hash(kilo,rd);
            } else
                hashes_[b] = Sha1Hash(hashes_[b.left()],hashes_[b.right()]);
        peak_hashes_[p] = hashes_[peaks_[p]];
        ack_out_.set(peaks_[p],bins::FILLED);
    }
    root_hash_ = DeriveRoot();
    Sha1Hash *hash_tmp = hashes_;
    SetSize(st.st_size);
    SavePeaks();
    seq_complete_ = complete_ = size_;
    completek_ = sizek_;
    memcpy(hashes_,hash_tmp,sizek_*Sha1Hash::SIZE*2);
    free(hash_tmp);
}


bin64_t         FileTransfer::peak_for (bin64_t pos) const {
    int pi=0;
    while (pi<peak_count_ && !pos.within(peaks_[pi]))
        pi++;
    return pi==peak_count_ ? bin64_t(bin64_t::NONE) : peaks_[pi];
}


void            FileTransfer::OfferHash (bin64_t pos, const Sha1Hash& hash) {
	if (!size_)  // only peak hashes are accepted at this point
		return OfferPeak(pos,hash);
    int pi=0;
    while (pi<peak_count_ && !pos.within(peaks_[pi]))
        pi++;
    if (pi==peak_count_)
        return;
    if (pos==peaks_[pi] && hash!=peak_hashes_[pi])
        return;
    else if (ack_out_.get(pos.parent())!=bins::EMPTY)
        return; // have this hash already, even accptd data
	hashes_[pos] = hash;
}


bin64_t         FileTransfer::data_in (int offset) {
    if (offset>data_in_.size())
        return bin64_t::NONE;
    return data_in_[offset];
}


bool            FileTransfer::OfferData (bin64_t pos, const uint8_t* data, size_t length) {
    if (!pos.is_base())
        return false;
    if (length<1024 && pos!=bin64_t(0,sizek_-1))
        return false;
    if (ack_out_.get(pos)==bins::FILLED)
        return true; // ???
    bin64_t peak = peak_for(pos);
    if (peak==bin64_t::NONE)
        return false;

    Sha1Hash hash(data,length);
    bin64_t p = pos;
    while ( p!=peak && ack_out_.get(p)==bins::EMPTY ) {
        hashes_[p] = hash;
        p = p.parent();
        hash = Sha1Hash(hashes_[p.left()],hashes_[p.right()]) ;
    }
    if (hash!=hashes_[p])
        return false;

    //printf("g %lli %s\n",(uint64_t)pos,hash.hex().c_str());
	// walk to the nearest proven hash   FIXME 0-layer peak
    ack_out_.set(pos,bins::FILLED);
    pwrite(fd_,data,length,pos.base_offset()<<10);
    complete_ += length;
    completek_++;
    if (length<1024) {
        size_ -= 1024 - length;
        ftruncate(fd_, size_);
    }
    while (ack_out_.get(bin64_t(0,seq_complete_>>10))==bins::FILLED)
        seq_complete_+=1024;
    if (seq_complete_>size_)
        seq_complete_ = size_;
    data_in_.push_back(pos);
    return true;
}


Sha1Hash        FileTransfer::DeriveRoot () {
	int c = peak_count_-1;
	bin64_t p = peaks_[c];
	Sha1Hash hash = peak_hashes_[c];
	c--;
	while (p!=bin64_t::ALL) {
		if (p.is_left()) {
			p = p.parent();
			hash = Sha1Hash(hash,Sha1Hash::ZERO);
		} else {
			if (c<0 || peaks_[c]!=p.sibling())
				return Sha1Hash::ZERO;
			hash = Sha1Hash(peak_hashes_[c],hash);
			p = p.parent();
			c--;
		}
        //printf("p %lli %s\n",(uint64_t)p,hash.hex().c_str());
	}
    return hash;
}


void            FileTransfer::OfferPeak (bin64_t pos, const Sha1Hash& hash) {
    assert(!size_);
    if (peak_count_) {
        bin64_t last_peak = peaks_[peak_count_-1];
        if ( pos.layer()>=last_peak.layer() ||
             pos.base_offset()!=last_peak.base_offset()+last_peak.width() )
            peak_count_ = 0;
    }
    peaks_[peak_count_] = pos;
    peak_hashes_[peak_count_++] = hash;
    // check whether peak hash candidates add up to the root hash
    Sha1Hash mustbe_root = DeriveRoot();
    if (mustbe_root!=root_hash_)
        return;
    // bingo, we now know the file size (rounded up to a KByte)
    SetSize( (pos.base_offset()+pos.width()) << 10		 );
    SavePeaks();
}


FileTransfer::~FileTransfer ()
{
#ifdef _WIN32
	UnmapViewOfFile(hashes_);
	CloseHandle(hashmaphandle_);
#else
    munmap(hashes_,sizek_*2*Sha1Hash::SIZE);
    close(hashfd_);
    close(fd_);
    files[fd_] = NULL;
#endif
}


FileTransfer* FileTransfer::Find (const Sha1Hash& root_hash) {
    for(int i=0; i<files.size(); i++)
        if (files[i] && files[i]->root_hash_==root_hash)
            return files[i];
    return NULL;
}



std::string FileTransfer::GetTempFilename(Sha1Hash& root_hash, int instance, std::string postfix)
{
	std::string tempfile = gettmpdir();
	std::stringstream ss;
	ss << instance;
	tempfile += std::string(".") + root_hash.hex() + std::string(".") + ss.str() + postfix;
	return tempfile;
}



int      p2tp::Open (const char* filename, const Sha1Hash& hash) {
    FileTransfer* ft = new FileTransfer(filename, hash);
    int fdes = ft->file_descriptor();
    if (fdes>0) {
        if (FileTransfer::files.size()<fdes)
            FileTransfer::files.resize(fdes);
        FileTransfer::files[fdes] = ft;
        return fdes;
    } else {
        delete ft;
        return -1;
    }
}


/*
 for(int i=0; i<peak_hash_count; i++) {
 bin64_t x = peaks[i], end = x.sibling();
 do {
 while (!x.layer()>10) {
 OfferHash(x.right(), hashes[x.right()]);
 if ( ! OfferHash(x.left(), hashes[x.left()]) )
 break;
 x = x.left();
 }

 if (x.layer()==10) {
 if (recheck_data) {
 uint8_t data[1024];
 size_t rd = pread(fd,data,2<<10,x.base_offset());
 if (hashes[x]==Sha1Hash(data,rd))
 ack_out->set(x,bins::FILLED);
 // may avoid hashing by checking whether it is zero
 // and whether the hash matches hash of zero
 } else {
 ack_out->set(x,bins::FILLED);
 }
 }

 while (x.is_right() && x!=peaks[i])
 x = x.parent();
 x = x.sibling();
 } while (x!=end);
 }




 // open file
 if ( hash_file_st.st_size < (sizeof(bin64_t)+Sha1Hash::SIZE)*64 )
 return;
 // read root hash
 char hashbuf[128];
 uint64_t binbuf;
 lseek(hashfd_,0,SEEK_SET);
 read(hashfd_,&binbuf,sizeof(bin64_t));
 read(hashfd_,hashbuf,Sha1Hash::SIZE);
 Sha1Hash mustberoot(false,(const char*)hashbuf);
 if ( binbuf!=bin64_t::ALL || mustberoot != this->root_hash ) {
 ftruncate(hashfd_,Sha1Hash::SIZE*64);
 return;
 }
 // read peak hashes
 for(int i=1; i<64 && !this->size; i++){
 read(hashfd_,&binbuf,sizeof(bin64_t));
 read(hashfd_,hashbuf,Sha1Hash::SIZE);
 Sha1Hash mustbepeak(false,(const char*)hashbuf);
 if (mustbepeak==Sha1Hash::ZERO)
 break;
 OfferPeak(binbuf,mustbepeak);
 }
 if (!size)
 return;


 */
