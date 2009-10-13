/*
 *  transfer.cpp
 *  p2tp
 *
 *  Created by Victor Grishchenko on 10/6/09.
 *  Copyright 2009 Delft Technical University. All rights reserved.
 *
 */
#include <sys/mman.h>
#include <errno.h>
#include "p2tp.h"

using namespace p2tp;

std::vector<FileTransfer*> FileTransfer::files(20);
const char* FileTransfer::HASH_FILE_TEMPLATE = "/tmp/.%s.%i.hashes";
const char* FileTransfer::PEAK_FILE_TEMPLATE = "/tmp/.%s.%i.peaks";
int FileTransfer::instance = 0;
#define BINHASHSIZE (sizeof(bin64_t)+sizeof(Sha1Hash))

#include "ext/seq_picker.cpp"


FileTransfer::FileTransfer (const Sha1Hash& _root_hash, const char* filename) :
    root_hash(_root_hash), fd(0), hashfd(0), dry_run(false), 
    peak_count(0), hashes(NULL), error(NULL), size(0), sizek(0),
    complete(0), completek(0), seq_complete(0)
{
	fd = open(filename,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (fd<0)
        return;
    if (root_hash==Sha1Hash::ZERO) // fresh submit, hash it
        Submit();
    else
        RecoverProgress();
    picker = new SeqPiecePicker(this);
}

void FileTransfer::LoadPeaks () {
    char file_name[1024];
    sprintf(file_name,PEAK_FILE_TEMPLATE,root_hash.hex().c_str(),instance);
    int peakfd = open(file_name,O_RDONLY);
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
    dry_run = true;
    LoadPeaks();
    if (!size)
        return;
    // at this point, we may use mmapd hashes already
    // so, lets verify hashes and the data we've got
    lseek(fd,0,SEEK_SET);
    for(int p=0; p<sizek; p++) {
        uint8_t buf[1<<10];
        size_t rd = read(fd,buf,1<<10);
        OfferData(bin64_t(0,p), buf, rd);
        if (rd<(1<<10))
            break;
    }
    dry_run = false;
}


void    FileTransfer::SavePeaks () {
    char file_name[1024];
    sprintf(file_name,PEAK_FILE_TEMPLATE,root_hash.hex().c_str(),instance);
    int peakfd = open(file_name,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    for(int i=0; i<peak_count; i++) {
        write(peakfd,&(peaks[i]),sizeof(bin64_t));
        write(peakfd,*peak_hashes[i],Sha1Hash::SIZE);
    }
    close(peakfd);
}


void FileTransfer::SetSize (size_t bytes) { // peaks/root must be already set
    size = bytes;
    completek = complete = seq_complete = 0;
	sizek = (size>>10) + ((size&1023) ? 1 : 0);
    
    char file_name[1024];
	struct stat st;
	fstat(fd, &st);
    if (st.st_size!=bytes)
        if (ftruncate(fd, bytes))
            return; // remain in the 0-state
    // mmap the hash file into memory
    sprintf(file_name,HASH_FILE_TEMPLATE,root_hash.hex().c_str(),instance);
	hashfd = open(file_name,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    size_t expected_size = Sha1Hash::SIZE * sizek * 2;
	struct stat hash_file_st;
	fstat(hashfd, &hash_file_st);
    if ( hash_file_st.st_size != expected_size )
        ftruncate(hashfd, expected_size);
    hashes = (Sha1Hash*) mmap (NULL, expected_size, PROT_READ|PROT_WRITE, 
                               MAP_SHARED, hashfd, 0);
    if (hashes==MAP_FAILED) {
        hashes = NULL;
        size = sizek = complete = completek = seq_complete = 0;
        error = strerror(errno);
        perror("hash tree mmap failed");
        return;
    }
    for(int i=0; i<peak_count; i++)
        hashes[peaks[i]] = peak_hashes[i];
}  


void            FileTransfer::Submit () {
	struct stat st; // TODO:   AppendData()   and   streaming
	fstat(fd, &st);
    size = st.st_size;
	sizek = (size>>10) + ((size&1023) ? 1 : 0);
    hashes = (Sha1Hash*) malloc(Sha1Hash::SIZE*sizek*2);
    peak_count = bin64_t::peaks(sizek,peaks);
    for (int p=0; p<peak_count; p++) {
        for(bin64_t b=peaks[p].left_foot(); b.within(peaks[p]); b=b.next_dfsio(0)) 
            if (b.is_base()) {
                uint8_t kilo[1<<10];
                size_t rd = pread(fd,kilo,1<<10,b.base_offset()<<10);
                hashes[b] = Sha1Hash(kilo,rd);
            } else
                hashes[b] = Sha1Hash(hashes[b.left()],hashes[b.right()]);
        peak_hashes[p] = hashes[peaks[p]];
        ack_out.set(peaks[p],bins::FILLED);
    }
    root_hash = DeriveRoot();
    Sha1Hash *hash_tmp = hashes;
    SetSize(st.st_size);
    SavePeaks();
    seq_complete = complete = size;
    completek = sizek;
    memcpy(hashes,hash_tmp,sizek*Sha1Hash::SIZE*2);
    free(hash_tmp);
}


void            FileTransfer::OfferHash (bin64_t pos, const Sha1Hash& hash) {    
	if (!size)  // only peak hashes are accepted at this point
		return OfferPeak(pos,hash);
	if (pos>=sizek*2)
		return;
    if (ack_out.get(pos)!=bins::EMPTY)
        return; // have this hash already, even accptd data
	hashes[pos] = hash;
}


bool            FileTransfer::OfferData (bin64_t pos, uint8_t* data, size_t length) {
    if (!pos.is_base())
        return false;
    if (length<1024 && pos!=bin64_t(0,sizek-1))
        return false;
    if (ack_out.get(pos)==bins::FILLED)
        return true; // ???
    int peak=0;
    while (peak<peak_count && !pos.within(peaks[peak]))
        peak++;
    if (peak==peak_count)
        return false;
    Sha1Hash hash(data,length);
    if (pos==peaks[peak]) {
        if (hash!=peak_hashes[peak])
            return false;
    } else {
        hashes[pos] = hash;
        for(bin64_t p = pos.parent(); p.within(peaks[peak]) && ack_out.get(p)==bins::EMPTY; p=p.parent()) {
            Sha1Hash phash = Sha1Hash(hashes[p.left()],hashes[p.right()]) ;
            if (hashes[p]!=phash)
                return false; // hash mismatch
        }
    }
    //printf("g %lli %s\n",(uint64_t)pos,hash.hex().c_str());
	// walk to the nearest proven hash   FIXME 0-layer peak
    ack_out.set(pos,bins::FILLED);
    pwrite(fd,data,length,pos.base_offset()<<10);
    complete += length;
    completek++;
    if (length<1024) {
        size -= 1024 - length;
        ftruncate(fd, size);
    }
    while (ack_out.get(bin64_t(0,seq_complete>>10))==bins::FILLED)
        seq_complete+=1024;
    if (seq_complete>size)
        seq_complete = size;
    return true;
}


Sha1Hash        FileTransfer::DeriveRoot () {
	int c = peak_count-1;
	bin64_t p = peaks[c];
	Sha1Hash hash = peak_hashes[c];
	c--;
	while (p!=bin64_t::ALL) {
		if (p.is_left()) {
			p = p.parent();
			hash = Sha1Hash(hash,Sha1Hash::ZERO);
		} else {
			if (c<0 || peaks[c]!=p.sibling())
				return Sha1Hash::ZERO;
			hash = Sha1Hash(peak_hashes[c],hash);
			p = p.parent();
			c--;
		}
        //printf("p %lli %s\n",(uint64_t)p,hash.hex().c_str());
	}
    return hash;
}


void            FileTransfer::OfferPeak (bin64_t pos, const Sha1Hash& hash) {
    assert(!size);
    if (peak_count) {
        bin64_t last_peak = peaks[peak_count-1];
        if ( pos.layer()>=last_peak.layer() || 
             pos.base_offset()!=last_peak.base_offset()+last_peak.width() )
            peak_count = 0;
    }
    peaks[peak_count] = pos;
    peak_hashes[peak_count++] = hash;
    // check whether peak hash candidates add up to the root hash
    Sha1Hash mustbe_root = DeriveRoot();
    if (mustbe_root!=root_hash)
        return;
    // bingo, we now know the file size (rounded up to a KByte)
    SetSize( (pos.base_offset()+pos.width()) << 10		 );
    SavePeaks();
}


FileTransfer::~FileTransfer () {
    munmap(hashes,sizek*2*Sha1Hash::SIZE);
    close(hashfd);
    close(fd);
}

                           
FileTransfer* FileTransfer::Find (const Sha1Hash& root_hash) {
    for(int i=0; i<files.size(); i++)
        if (files[i] && files[i]->root_hash==root_hash)
            return files[i];
    return NULL;
}


int      p2tp::Open (const char* filename) {
    return Open(Sha1Hash::ZERO,filename);
}


int      p2tp::Open (const Sha1Hash& hash, const char* filename) {
    FileTransfer* ft = new FileTransfer(hash, filename);
    if (ft->fd>0) {
        if (FileTransfer::files.size()<ft->fd)
            FileTransfer::files.resize(ft->fd);
        FileTransfer::files[ft->fd] = ft;
        return ft->fd;
    } else {
        delete ft;
        return -1;
    }
}


void     Close (int fdes) {
    // FIXME delete all channels
    delete FileTransfer::files[fdes];
    FileTransfer::files[fdes] = NULL;
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
 lseek(hashfd,0,SEEK_SET);
 read(hashfd,&binbuf,sizeof(bin64_t));
 read(hashfd,hashbuf,Sha1Hash::SIZE);
 Sha1Hash mustberoot(false,(const char*)hashbuf);
 if ( binbuf!=bin64_t::ALL || mustberoot != this->root_hash ) {
 ftruncate(hashfd,Sha1Hash::SIZE*64);
 return;
 }
 // read peak hashes
 for(int i=1; i<64 && !this->size; i++){
 read(hashfd,&binbuf,sizeof(bin64_t));
 read(hashfd,hashbuf,Sha1Hash::SIZE);
 Sha1Hash mustbepeak(false,(const char*)hashbuf);
 if (mustbepeak==Sha1Hash::ZERO)
 break;
 OfferPeak(binbuf,mustbepeak);
 }
 if (!size)
 return;
 
 
 */
